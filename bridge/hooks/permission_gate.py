#!/usr/bin/env python3
"""
permission_gate.py  —  忠实放行 hook（挂在 PermissionRequest）
  PermissionRequest 只在 CC *真正需要你批准* 某工具时触发（不像 PreToolUse 每个工具都触发），
  所以这是"该出挑战"的唯一忠实时机 —— 修掉"不等你就开游戏"。

  流程（仅 /claudee 激活时）：
    1. 把工具描述(如 'Bash: npm test')作为情境行发给板子；
    2. 让板子进 NEEDPERM 并阻塞等板子回 'A'/'D'/超时；
    3. 'A' -> 输出 allow ；'D' -> 输出 deny ；超时/不可达 -> 静默(回落 CC 正常弹窗)。

  注意：PermissionRequest 用 hookSpecificOutput.decision.behavior（与 PreToolUse 不同）。
  已知某些 CC 版本 deny 可能被忽略(#19298)——deny 仍照发，被忽略时 CC 会回落正常弹窗，安全。
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
        return 0    # 未激活：放过，CC 正常权限流程

    desc = P.tool_descriptor(data.get("tool_name", ""), data.get("tool_input"))
    C.send({"cmd": "context", "text": desc})

    reply = C.send({"cmd": "wait_accept", "timeout": ACCEPT_TIMEOUT},
                   expect_reply=True, read_timeout=ACCEPT_TIMEOUT + 30.0)
    decision = reply.get("decision", "ask") if reply.get("ok") else "ask"
    if decision == "allow":
        print(json.dumps(P.permreq_decision(True)))
    elif decision == "deny":
        print(json.dumps(P.permreq_decision(False)))
    # else ask/超时：不输出 -> CC 回落正常弹窗
    return 0


if __name__ == "__main__":
    sys.exit(main())
