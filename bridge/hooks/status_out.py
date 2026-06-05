#!/usr/bin/env python3
"""
status_out.py  —  非阻塞状态上报 hook
用法(settings.json): python3 status_out.py [code]
  显式 code: 1 STARTED / 2 WORKING / 3 IDLE / 7 ASK
  无 code  : 从事件 JSON(hook_event_name + matcher)推断
若未 /claudee 激活则什么都不做(不打扰)。
ASK(CC 多选一)时额外把问题文本作为情境行发给板子(板上只提示"去终端选")。
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import claudee_client as C
import protocol as P


def main():
    raw = ""
    try:
        raw = sys.stdin.read()
    except Exception:
        pass
    try:
        data = json.loads(raw or "{}")
    except Exception:
        data = {}

    if not C.is_active():
        return 0    # 未激活：静默

    code = None
    if len(sys.argv) > 1:
        try:
            code = int(sys.argv[1])
        except ValueError:
            code = None
    if code is None:
        code = P.status_code_for(data.get("hook_event_name"), data.get("matcher"))

    if code is None:
        return 0

    C.send({"cmd": "status", "code": code})
    if code == P.ASK:
        q = data.get("message") or data.get("title") or "Choose in terminal"
        C.send({"cmd": "context", "text": str(q)})
    return 0


if __name__ == "__main__":
    sys.exit(main())
