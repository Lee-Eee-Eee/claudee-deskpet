<div align="center">

# Claudee 🦀

**把 Claude Code 变成你桌上的硬件小宠物**

一块清华 Blazar（NXP MKL25Z128，Cortex-M0+）教学板 + 一只像素螃蟹 **Clawd**：
它实时显示 Claude Code 的工作状态与 token/花费，
当 CC 请求工具权限时，让你**做满 10 个深蹲**或**玩赢一局跑酷**来"放行"。

裸机 C · 无操作系统 · 无 framebuffer · GNU Make + arm-none-eabi-gcc

</div>

---

## ✨ 这是什么

`Claude Code`（CC）是命令行里的 AI 编程助手。Claudee 把它"实体化"：

- **状态联动**——CC 在思考/敲代码/空闲/求批准时，板上的 Clawd 会换表情、挥钳子、转 Claude ✶、轮播俏皮话；4 颗板载 LED 随状态呼吸/快闪。
- **实时遥测**——屏幕底部显示本次会话的 **模型 / 累计花费 / token 数 / 上下文占用%**（`Opus  $0.58  51.1k  5%`）。
- **权限即挑战**——当且仅当 CC **真的发出权限请求**（要跑 `Bash`、改文件……）时，板子进入挑战：
  - **深蹲模式**：举着板子做 10 个深蹲（加速度计检测），完成 → 回传 `accept`，CC 继续；
  - **跑酷模式**：操纵 Clawd 跳过/躲开 10 个对手 AI 的图标（OpenAI / Gemini / Grok / Codex / DeepSeek / Kimi / Antigravity / opencode / openclaw / Claude），攒够 10 分按键放行。
- **本地可玩**——三个按键可不经 CC 直接唤起挑战、切换模式、跳跃，方便调试与展示。

> 名字来源：**Clawd** 是 Claude Code 的橙色像素螃蟹吉祥物，**Claudee** = Clawd 的硬件化桌宠。

---

## 🧩 三大组件

整个系统由三部分串成一条链路（CC 在 WSL，串口在 Windows，挑战在板子）：

```
   WSL (你在这用 Claude Code)            Windows                       Claudee 板 (KL25Z128)
 ┌──────────────────────────┐ localhost ┌────────────────────┐  COMx  ┌────────────────────────┐
 │ ① Claude Code + skill     │   TCP     │ ② bridge.py        │ UART1  │ ③ 固件 (firmware/)      │
 │   hooks / statusLine ─────┼──状态码──►│   独占 COM 口       │ 9600   │   RX 中断 → 画 Clawd    │
 │   /claudee 命令           │◄─accept── │   状态→串口/等放行   │ ─────► │   权限→挑战→回传 'A'/'D' │
 └──────────────────────────┘           └────────────────────┘        └────────────────────────┘
```

| # | 组件 | 目录 | 跑在哪 | 职责 |
|---|---|---|---|---|
| ① | **CC Skill**（Claude Code 集成） | `bridge/hooks/` + `bridge/commands/claudee.md` + `bridge/settings.snippet.json` | WSL | `/claudee` 命令开关；hooks 把 CC 状态/遥测上报、在 `PermissionRequest` 时阻塞等板子放行；statusLine 显示并转发 token/花费 |
| ② | **Win/WSL Bridge** | `bridge/bridge.py` + `bridge/protocol.py` | Windows | 独占串口；本地 TCP 服务（让 WSL 的 hooks 经 localhost 连过来）；状态码→串口；阻塞等板子回传 accept |
| ③ | **硬件固件** | `firmware/` | KL25Z 板 | 裸机 C：UART 收状态、TFT 画 Clawd/跑酷、加速度计数深蹲、LED 呼吸、蜂鸣器音效、按键 |

> ①②同处 `bridge/` 一个目录，是因为 `install.sh` 会一次性把 hooks/命令装进 `~/.claude` 并配好 `settings.json`——拆开会破坏它依赖的相对路径。逻辑上它们仍是两个独立组件。

---

## 🛠 硬件平台

| 外设 | 器件 / 引脚 | 课程实验来源 |
|---|---|---|
| MCU | NXP **MKL25Z128**（Cortex-M0+，48MHz 上限，**本项目用默认 FEI ~20.97MHz**，16KB SRAM，无 FPU） | — |
| 显示 | **ILI9341** 320×240 TFT，8 位并口（PTD0–7 + 控制脚 PTB8–11/PTA19/PTE31） | 7.1 |
| 加速度计 | **MMA8451Q**，I²C0（PTE24/25，地址 0x1D） | 7.1 / 8.x |
| 串口 | UART1（PTC3 RX / PTC4 TX，9600 8N1） | 1.2 |
| 蜂鸣器 | TPM0_CH4 PWM（PTC8） | 8.2 |
| 音量旋钮 | ADC0_SE14（PTC0，电位器） | 8.2 / 6.1 |
| 按键 | PORTA 中断 A=PTA14 / B=PTA16 / C=PTA17 | 3.2 |
| 板载 4 LED | PORTC（组选 PTC6/7/12/13 + 红 PTC9），PIT 软件 PWM 呼吸 | 4.2 |

无 framebuffer（16KB SRAM 放不下 320×240×2B=150KB），所有画面都是**局部/脏矩形直写面板 GRAM**。

---

## 🎓 用到的课堂知识点（大作业重点）

> 这是一台几乎把嵌入式课程"全家桶"都用上的设备。下表把每个知识点对到**代码位置**。

| 课堂知识点 | 在 Claudee 里怎么用 | 代码 |
|---|---|---|
| **GPIO 输入/输出 + PORT 复用(PCR/MUX)** | LCD 8 位并口、4 LED、3 按键、LCD 控制脚 | `firmware/Sources/led.c`、`input.c`、`Blazar_TFTLCD.c` |
| **中断 + NVIC + 向量表(weak 别名覆盖)** | UART1 RX(**IRQ13**)、PORTA 按键(**IRQ30**)、PIT(**IRQ22**) 三个 ISR 各自覆盖 `kinetis_sysinit.c` 里的弱符号 | `comm.c`、`input.c`、`led.c`、`Project_Settings/Startup_Code/kinetis_sysinit.c` |
| **UART 异步串口** | 与 PC 双向 1 字节协议；BDH/BDL 配 9600；RX 中断收状态码 | `comm.c`、`UART.c` |
| **I²C 总线** | 读 MMA8451Q 三轴加速度做深蹲检测 | `MMA8451Q.c`、`squat.c` |
| **ADC 模数转换** | 旋钮电位器 → 12 位采样 → 映射蜂鸣器音量 | `sfx.c`（`ADC0_SE14`） |
| **定时器 / PWM** | TPM0 输出蜂鸣器音调；PIT 周期中断做 LED 软件 PWM；SysTick 1ms 时基 | `sfx.c`、`led.c`、`timebase.c` |
| **时钟树 / FEI** | 全程用复位默认 FEI（core ~20.97MHz / bus ~10.49MHz），据此算 SysTick 重载值、UART 波特率、PIT 周期 | `timebase.c`、`comm.c`、`led.c` |
| **存储器映射 I/O / 寄存器级编程** | 直接读写 `SIM_SCGC*`、`PORTx_PCRn`、`GPIOx_PDOR/PDDR`、`TPM0_*`、`ADC0_*`、`UART1_*`、`PIT_*` | 全部 `firmware/Sources/*.c` |
| **临界区 / 并发** | `cpsid/cpsie i` 保护 ISR 与主循环共享数据；`volatile`；LED 故意只用 PORTC 避免与 LCD 抢 `GPIOB` 的读改写 | `comm.c`、`led.c`、`input.c` |
| **裸机状态机 / 非阻塞主循环** | 顶层 `NORMAL ⇄ WAIT` 状态机；音效/动画/采样都非阻塞分时 | `main.c` |
| **定点数 / 避免浮点(无 FPU)** | 深蹲用"平方幅值 + 迟滞"判定（不开根号）；跑酷整数物理 | `squat.c`、`game.c` |
| **面板时序 / 显存寻址** | ILI9341 的 `CASET/PASET/MemWrite`、`MADCTL` 扫描方向；开窗流式 vs 逐像素 | `gfx.c`、`Blazar_TFTLCD.c` |
| **串口通信协议设计** | 1 字节状态码 + `0xFE` 遥测帧 + `0xFC` 情境帧 + 链路超时回 SLEEP | `comm.c`、`bridge/protocol.py` |

更细的实现与取舍记录在各模块源码注释里（中文）。

---

## 📡 通信协议（串口，9600 8N1）

| PC → 板 | 含义 | | 板 → PC | 含义 |
|---|---|---|---|---|
| `0x00` | SLEEP / 未连接 | | `'A'` (0x41) | accept（挑战通过）|
| `0x01` | STARTED 来活了 | | `'D'` (0x44) | deny（放弃/失败）|
| `0x02` | WORKING 忙 | | | |
| `0x03` | IDLE 空闲 | | | |
| `0x04` | NEEDPERM 求批准 | | | |
| `0x07` | ASK 多选一（提示去终端选）| | | |

- **遥测帧**：`0xFE` + ASCII(≤31，如 `Opus $0.58 51.1k 5%`) + `0x0A`
- **情境帧**：`0xFC` + ASCII(工具名 / ASK 问题) + `0x0A`
- 板子 **>3s 收不到任何字节 → 自动回 SLEEP**（拔线/CC 退出也能正确睡）。

---

## 🚀 快速开始

### 1) 烧录固件（Windows）
```bat
cd firmware
make                         &:: 需 arm-none-eabi-gcc + make
:: 或在 VS Code 里 F5 经 J-Link 烧录 (build/app.bin)
```
> WSL 里只能 `make`/语法检查，**不能 J-Link 烧录**（J-Link 在 Windows 侧）。

### 2) 接线
板 `PTC4(TX)→PC RX`、`PTC3(RX)→PC TX`、共地，9600 8N1（和用 MobaXterm 连板时一样；跑 Claudee 时**先关掉 MobaXterm**，COM 口要交给 bridge 独占）。

### 3) 装 CC skill（在 WSL 里）
```bash
cd bridge
./install.sh          # 装 hooks + /claudee 命令, 自动并入 ~/.claude/settings.json
# 重启 Claude Code 使 settings 生效
```

### 4) 跑桥（Windows）
```bat
pip install pyserial
python bridge\bridge.py --com COM5        :: 换成你的串口号；无板子可 --dry-run
```

### 5) 用起来
```
/claudee            ← 激活，Clawd 醒来
/claudee strict     ← (可选) 严格"游戏化"，拦更多工具
... 正常使用 CC ...   ← Clawd 随状态变脸 + 屏底显示 token/花费
   当 CC 求权限 → 板子进挑战 → 深蹲满 10 / 跑酷赢 → CC 自动继续
/claudee off        ← 关掉，Clawd 回去睡
```

**本地测试（不必经 CC）**：NORMAL 屏按 **B** 选深蹲/跑酷 → 按 **A** 直接进挑战 → 跑酷里 **C** 跳。

---

## 📁 目录结构

```
claudee-deskpet/
├─ README.md
├─ LICENSE                 (MIT)
├─ firmware/               ③ 硬件固件 (KL25Z, 裸机 C)
│  ├─ Sources/             自写模块 + 复用驱动 (.c/.h)
│  ├─ Project_Headers/     NXP 设备头 (MKL25Z4.h, derivative.h)
│  ├─ Project_Settings/    启动代码 + 链接脚本 (向量表在此)
│  └─ Makefile             arm-none-eabi-gcc + J-Link
├─ bridge/                 ①CC skill + ②Win/WSL 桥
│  ├─ bridge.py            Windows 串口守护 + TCP 服务
│  ├─ protocol.py          两端共用的帧协议 (+单测)
│  ├─ hooks/               CC hooks (permission_gate/status_out/statusline/ctl/client)
│  ├─ commands/claudee.md  /claudee 命令
│  ├─ settings.snippet.json  待并入的 hooks + statusLine
│  ├─ install.sh           一键安装 skill
│  └─ tests/               协议单测
└─ (slides / 设计文档不入库；slides 在仓库外，设计笔记见 firmware/AGENTS.md)
```

## 🔧 构建与测试

```bash
# 固件语法检查 (任意平台, 不需 J-Link):
cd firmware && for f in Sources/*.c; do \
  gcc -fsyntax-only -std=gnu99 -Wall -Wextra -Wno-unused-parameter \
      -DCPU_MKL25Z128VLK4 -IProject_Headers -IProject_Settings/Startup_Code -ISources "$f"; done

# 协议单测:
cd bridge && python3 -m pytest tests/ -q
```

## 🙏 复用与致谢

- 复用清华 Blazar 课程实验驱动：`Blazar_TFTLCD.c`(7.1 TFT)、`MMA8451Q.c`、`KL2x_gpio.c`、`UART.c`(1.2)、PORTA 按键中断(3.2)、TPM/ADC(8.2)、LED 接线(4.2)。
- NXP/Freescale 设备头与启动代码位于 `firmware/Project_Headers/`、`firmware/Project_Settings/`。
- 上述第三方文件保留各自原始许可；本仓库其余原创代码以 MIT 许可发布（见 `LICENSE`，可把版权人改成你的名字）。

---

<div align="center">
🦀 <i>Clawd 在等你来撸（的代码）。</i>
</div>
