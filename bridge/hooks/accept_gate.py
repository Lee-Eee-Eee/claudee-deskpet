#!/usr/bin/env python3
"""
accept_gate.py  —  PreToolUse hook
  职责一: 上报"忙"(WORKING)，让 Clawd 在 CC 跑工具时显示工作态。
  职责二(仅严格/游戏化模式 /claudee strict): 主动拦受控工具集(Bash/Edit/Write…)，
          阻塞等板子挑战放行 -> allow/deny；非严格模式从不拦截(只上报忙)。

  忠实模式(默认)下本 hook 绝不阻塞 CC —— 真正的权限挑战交给 permission_gate(PermissionRequest)。

  注意: 某些 CC 版本 PreToolUse 的 allow 可能压不住原生弹窗(#52822)，严格模式属实验性。
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import claudee_client as C
import protocol as P

ACCEPT_TIMEOUT = float(os.environ.get("CLAUDEE_ACCEPT_TIMEOUT", "300"))


def main():
    try:
        data = json.loads(sys.stdin.read() or "{}")
    except Exception:
        data = {}

    if not C.is_active():
        return 0    # 未激活：放过

    tool = data.get("tool_name", "")
    C.send({"cmd": "status", "code": P.WORKING})   # 工具运行中 -> 忙

    # 严格模式才主动拦截；否则到此为止(放过)
    if not (C.is_strict() and P.is_gated(tool)):
        return 0

    C.send({"cmd": "context", "text": P.tool_descriptor(tool, data.get("tool_input"))})
    reply = C.send({"cmd": "wait_accept", "timeout": ACCEPT_TIMEOUT},
                   expect_reply=True, read_timeout=ACCEPT_TIMEOUT + 30.0)
    decision = reply.get("decision", "ask") if reply.get("ok") else "ask"
    if decision == "allow":
        print(json.dumps(P.pretool_decision(True)))
    elif decision == "deny":
        print(json.dumps(P.pretool_decision(False)))
    # else ask/超时：静默回落 CC 正常询问
    return 0


if __name__ == "__main__":
    sys.exit(main())
