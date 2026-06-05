# Claudee Bridge (PC 端)

Claudee 桌宠的 PC 侧：把 **Claude Code** 的实时状态喂给板子上的像素螃蟹 **Clawd**，
并让你在板子上"做满 10 个深蹲 / 玩赢小游戏"来**批准** CC 的工具请求。
屏底还实时显示本次会话的 **token 用量与花费**（经 statusLine）。

```
  WSL (你在这用 CC)                Windows                         Claudee 板子 (KL25Z)
┌────────────────────┐ localhost ┌─────────────────────┐  COMx   ┌──────────────────────┐
│ Claude Code         │   TCP     │ bridge.py           │ UART1   │ 固件                  │
│  hooks 触发  ───────┼──状态码──►│ 独占 COM 口          │ 9600    │ RX中断 → 画 Clawd     │
│  PreToolUse(阻塞)   │◄─allow/── │ 状态→串口/阻塞等放行 │ ──────► │ 权限→挑战→回传 'A'    │
│  Stop/Notification  │   deny    │                     │◄─'A'/'D'┤                      │
└────────────────────┘           └─────────────────────┘         └──────────────────────┘
```

## 组成

| 文件 | 跑在哪 | 作用 |
|---|---|---|
| `bridge.py` | **Windows** | 独占 COM 口；本地 TCP 服务；状态→串口；阻塞等板子放行 |
| `protocol.py` | 两端共用 | 协议常量 + 映射（含单测） |
| `hooks/status_out.py` | WSL | 非阻塞上报状态（UserPromptSubmit/PostToolUse/Stop/Notification） |
| `hooks/accept_gate.py` | WSL | 受控工具(PreToolUse) 阻塞等板子放行 |
| `hooks/claudee_statusline.py` | WSL | statusLine：取 model/花费/token/ctx → 终端状态栏 + 发遥测给桥 |
| `hooks/claudee_ctl.py` | WSL | `/claudee on/off/status` 的执行体 |
| `hooks/claudee_client.py` | WSL | hook 共用：激活标记 + 连桥 |
| `commands/claudee.md` | WSL | `/claudee` 自定义命令 |
| `settings.snippet.json` | WSL | 待并入 `~/.claude/settings.json` 的 `hooks` + `statusLine` 段 |

## 协议（串口，每条 1 字节）

| PC→板 | 含义 |   | 板→PC | 含义 |
|---|---|---|---|---|
| `0x00` | SLEEP 睡 / 未连接 |   | `'A'` | accept（挑战通过）|
| `0x01` | STARTED 来活了 |   | `'D'` | deny（放弃/失败）|
| `0x02` | WORKING 忙 |   | | |
| `0x03` | IDLE 在线空闲 |   | | |
| `0x04` | NEEDPERM 求批准 |   | | |

板子若 >3s 收不到任何字节 → 自动回 SLEEP（拔线/CC 退出也能正确睡）。

**遥测帧**（token/花费/上下文，PC→板，不占状态字节）：`0xFE` + ASCII(≤31，如 `Opus $0.42 18.7k 8%`) + `0x0A`。
由 `claudee_statusline.py` 从 CC 的 statusLine JSON 取 `cost.total_cost_usd`（累计花费）、`context_window` 的 token 与 `used_percentage`（当前上下文）组装，桥转发到板底显示。

---

## 安装与运行

### 1. 烧录固件（Windows）
在 **Windows** 用 VS Code 打开 `Claudee/`，`make` 编译 + F5 经 J-Link 烧录（WSL 里无法 build）。

### 2. 接线（3.3V USB-TTL，或你已在用的板载串口）
板 `PTC4(TX)→PC RX`、`PTC3(RX)→PC TX`、共地，9600 8N1。
> 跑 Claudee 时 COM 口要交给 `bridge.py` 独占。

### 3. 装 hooks + 命令（WSL）
```bash
cd tools/claudee-bridge
./install.sh
```
然后把 `settings.snippet.json` 里的 `"hooks"` + `"statusLine"` 段合并进 `~/.claude/settings.json`。
（`statusLine` 把 token/花费/上下文% 同步到板底，也会成为你终端的状态栏。）

### 4. 跑桥（Windows）
```powershell
pip install pyserial
python tools\claudee-bridge\bridge.py --com COM5
```
（`--com` 换成你的串口号；不接板子可先 `--dry-run` 看打印。）

### 5. 用起来
在 WSL 的 Claude Code 里：
```
/claudee            ← 激活，Clawd 醒来开始反应
... 正常使用 CC ...  ← 螃蟹随状态变脸 + 轮播俏皮词；屏底实时显示 token/花费/上下文%
（当 CC 要跑 Bash/改文件等"受控工具"时，板子进挑战）
   → 做满 10 个深蹲 / 玩赢小游戏 → CC 自动获批继续
/claudee off        ← 关掉，螃蟹回去睡觉
```

---

## WSL ↔ Windows 网络

- **镜像网络模式**（Win11 较新）：hooks 直接连 `localhost:8787`，开箱即用。
- **NAT 模式**：客户端会自动尝试 `/etc/resolv.conf` 里的 nameserver（即 Windows 主机 IP）。
  如仍连不上，手动设环境变量：`export CLAUDEE_BRIDGE=<Windows-IP>:8787`。
- 端口默认 8787，可用 `CLAUDEE_PORT` 或 `bridge.py --port` 改。

## 不接板子也能联调（dry-run）
```bash
# 终端 A (可在 WSL 或 Windows)：
python bridge.py --dry-run            # 串口写会打印到屏幕
# 终端 B：模拟 /claudee 与状态，甚至模拟板子回传 accept
python hooks/claudee_ctl.py on
python -c "import hooks.claudee_client as C; C.send({'cmd':'status','code':2})"
python -c "import hooks.claudee_client as C; print(C.send({'cmd':'wait_accept','timeout':5}, expect_reply=True, read_timeout=10))" &
python -c "import hooks.claudee_client as C; C.send({'cmd':'inject','byte':'A'})"   # 模拟板子按下/蹲满
```

## 测试
```bash
python3 -m pytest tests/ -q        # 有 pytest 时
# 或无 pytest：
python3 - <<'PY'
import importlib.util; s=importlib.util.spec_from_file_location("t","tests/test_protocol.py")
m=importlib.util.module_from_spec(s); s.loader.exec_module(m)
[getattr(m,n)() for n in dir(m) if n.startswith("test_")]; print("ok")
PY
```

## 排错
- **Clawd 不动**：确认 (1) 板子已烧录并上电、(2) `bridge.py` 在 Windows 跑且 `--com` 正确、(3) 已 `/claudee`、(4) `~/.claude/settings.json` 已并入 hooks、(5) 网络（见上）。
- **每个操作都要我深蹲**：受控集太大。在 `protocol.py` 的 `DEFAULT_GATED` 里调小（或留 `Bash` 即可）。
- **CC 卡住不动**：accept 超时默认 300s 后会回落正常询问；可 `bridge.py --accept-timeout` 调短。
- **COM 抢占**：确认没有别的程序（MobaXterm/串口助手）占着同一个 COM。

## 受控工具集
默认 `Bash / Write / Edit / MultiEdit / NotebookEdit` 会触发挑战放行；只读类（Read/Grep/Glob…）一律放过、只上报"忙"。改 `protocol.py` 的 `DEFAULT_GATED` 调整。
