#!/usr/bin/env python3
"""
claudee_statusline.py  —  Claude Code statusLine 命令

作用：
  * 读 statusLine 在 stdin 给的 JSON -> 取 模型/累计花费$/上下文 token/上下文% ；
  * 打印一行给终端 statusLine 用（桌宠与终端同步显示）；
  * 若已 /claudee 激活：把同样的遥测发给桥 -> 板上 Clawd 屏底显示。

配置（~/.claude/settings.json）：
  "statusLine": { "type": "command",
                  "command": "python3 \"$HOME/.claude/claudee/hooks/claudee_statusline.py\"" }

字段（见 code.claude.com/docs/en/statusline）：
  model.display_name；cost.total_cost_usd(累计)；
  context_window.total_input_tokens / total_output_tokens / used_percentage。
  注：自 v2.1.132，token 字段是“当前上下文”而非累计；花费是累计。
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))            # hooks/
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # protocol.py
import claudee_client as C
import protocol as P


def main():
    try:
        data = json.loads(sys.stdin.read() or "{}")
    except Exception:
        data = {}

    model = (data.get("model") or {}).get("display_name") or "?"
    cost = (data.get("cost") or {}).get("total_cost_usd") or 0
    cw = data.get("context_window") or {}
    tokens = (cw.get("total_input_tokens") or 0) + (cw.get("total_output_tokens") or 0)
    pct = cw.get("used_percentage") or 0

    # 终端 statusLine（始终打印，作为用户的状态栏）
    try:
        line = "\U0001F980 %s  $%.2f  %s tok  %d%% ctx" % (
            str(model).split()[0] if str(model).split() else "?",
            float(cost or 0), P.humanize_tokens(tokens), int(float(pct or 0)))
    except Exception:
        line = "\U0001F980 Claudee"
    sys.stdout.write(line + "\n")

    # 发桥（仅 /claudee 激活时；失败静默，不打扰）
    if C.is_active():
        try:
            C.send({"cmd": "telemetry", "text": P.format_telemetry(model, cost, tokens, pct)})
        except Exception:
            pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
