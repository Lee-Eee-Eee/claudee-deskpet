#!/usr/bin/env python3
"""
claudee_ctl.py  —  /claudee 命令的实际执行体
  on      : 写激活标记 + 通知桥 activate
  off     : 删标记 + 通知桥 deactivate（板子回睡）
  strict  : 切换严格/游戏化模式(主动拦受控工具，常出挑战); 可 'strict on'/'strict off'
  status  : 查询桥状态 + 是否激活/严格
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import claudee_client as C
import protocol as P


def main():
    action = (sys.argv[1] if len(sys.argv) > 1 else "on").strip().lower()
    arg2 = (sys.argv[2] if len(sys.argv) > 2 else "").strip().lower()

    if action in ("on", "start", "activate", ""):
        C.set_active(True)
        r = C.send({"cmd": "activate", "session": os.environ.get("CLAUDE_SESSION_ID", "")},
                   expect_reply=True)
        if r.get("ok"):
            print("\U0001F980 Claudee armed — the crab reacts to this session. "
                  "(challenges only when CC asks permission; /claudee strict for more)")
        else:
            print("\U0001F980 Claudee armed (marker set). Bridge not reachable (%s) — "
                  "start bridge.py on Windows, or set CLAUDEE_BRIDGE=<winip>:8787." % r.get("error"))
    elif action in ("off", "stop", "deactivate"):
        C.set_active(False)
        C.send({"cmd": "deactivate"})
        print("\U0001F634 Claudee off — the crab goes back to sleep.")
    elif action in ("strict", "game"):
        on = (arg2 == "on") if arg2 in ("on", "off") else (not C.is_strict())
        C.set_strict(on)
        if on:
            print("\U0001F4AA Strict mode ON — Claudee now gates %s (squat/game to allow)."
                  % ", ".join(P.DEFAULT_GATED))
        else:
            print("\U0001F340 Strict mode OFF — challenges only on real permission prompts.")
    elif action in ("status", "ping"):
        r = C.send({"cmd": "ping"}, expect_reply=True)
        print("Claudee active=%s strict=%s | bridge=%s"
              % (C.is_active(), C.is_strict(), r))
    else:
        print("usage: claudee_ctl.py [on|off|strict [on|off]|status]")
    return 0


if __name__ == "__main__":
    sys.exit(main())
