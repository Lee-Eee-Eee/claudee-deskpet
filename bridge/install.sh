#!/usr/bin/env bash
# install.sh — 安装 Claudee 的 hooks + /claudee 命令到 ~/.claude（在 WSL 里运行）
# 并把 settings.snippet.json 的 statusLine + hooks 自动并入 ~/.claude/settings.json。
# 注意：bridge.py 跑在 *Windows* 侧（独占 COM 口），不在此安装。
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
DEST="$HOME/.claude/claudee"
CMDDIR="$HOME/.claude/commands"
SETTINGS="$HOME/.claude/settings.json"

mkdir -p "$DEST/hooks" "$CMDDIR"
cp "$HERE/protocol.py"      "$DEST/"
cp "$HERE/hooks/"*.py       "$DEST/hooks/"
chmod +x "$DEST/hooks/"*.py 2>/dev/null || true
# /claudee 命令：更新为仓库里的最新正确版（无 ! 宏 / 无 shell 展开，走 Bash 工具）。
# 仓库版已是修好的，所以照常更新；先把你已有的备份成 .bak，更新不丢改动。
if [ -f "$CMDDIR/claudee.md" ]; then
  cp "$CMDDIR/claudee.md" "$CMDDIR/claudee.md.bak"
  echo "ℹ️  旧 claudee.md 已备份为 claudee.md.bak，并更新为最新修复版。"
fi
cp "$HERE/commands/claudee.md" "$CMDDIR/"

# --- 合并 statusLine + hooks 到 settings.json（幂等：先移除旧的 claudee handler 再并入，
#     不动用户其它 hooks；若已存在则备份 .claudee.bak）---
python3 - "$HERE/settings.snippet.json" "$SETTINGS" <<'PY'
import json, sys, os, shutil
snip_path, settings_path = sys.argv[1], sys.argv[2]
snip = json.load(open(snip_path)); snip.pop("_comment", None)
settings = {}
if os.path.exists(settings_path):
    try: settings = json.load(open(settings_path))
    except Exception: settings = {}
    shutil.copy(settings_path, settings_path + ".claudee.bak")
if "statusLine" in snip:
    settings["statusLine"] = snip["statusLine"]
def is_claudee(h):
    return any("claudee" in str(x.get("command", "")) for x in h.get("hooks", []))
sh = settings.get("hooks", {})
for ev, arr in snip.get("hooks", {}).items():
    sh[ev] = [h for h in sh.get(ev, []) if not is_claudee(h)] + arr   # 去重 + 保留他人 hook
settings["hooks"] = sh
os.makedirs(os.path.dirname(settings_path), exist_ok=True)
json.dump(settings, open(settings_path, "w"), indent=2, ensure_ascii=False)
print("  merged statusLine + hooks ->", settings_path)
PY

echo "✅ 已安装并配置："
echo "   $DEST/protocol.py"
echo "   $DEST/hooks/{claudee_client,status_out,accept_gate,permission_gate,claudee_ctl,claudee_statusline}.py"
echo "   $CMDDIR/claudee.md   ( -> /claudee 命令)"
echo "   合并 statusLine(token/花费/上下文) + hooks(含 PermissionRequest 忠实放行) 到 settings.json"
echo
echo "下一步："
echo "  1) 在 Windows 上跑桥:  python tools\\claudee-bridge\\bridge.py --com COMx"
echo "     (先 pip install pyserial; 无板子可先 --dry-run；WSL 内 dry-run 则直接 localhost)"
echo "  2) 在 CC 里输入 /claudee 激活；/claudee strict 开严格(游戏化)模式。"
echo "  3) 重启 Claude Code 使 settings.json 生效。"
