"""
protocol.py  —  Claudee PC<->板 协议常量 + 纯映射函数（可单测）

串口字节 (PC -> 板):
    0x00 SLEEP / 0x01 STARTED / 0x02 WORKING / 0x03 IDLE / 0x04 NEEDPERM
    0x05 SUBAGENT / 0x06 DONE / 0x07 ASK(CC 在让你多选一，去终端选)
帧 (PC -> 板, 不与状态字节冲突):
    0xFE <ascii> 0x0A   遥测行(token/cost/ctx，屏底)
    0xFC <ascii> 0x0A   情境行(挑战时的工具描述 / ASK 的问题)，显示在挑战/ASK 屏
板 -> PC: b'A' = accept(挑战通过/允许), b'D' = deny(拒绝)

桥<->hook 走本地 TCP，行分隔 JSON（见 bridge.py）。

交互模型（2026-06-05 重构）:
  * 忠实(默认): 挑战只在 CC 真发 PermissionRequest 时出现 -> permission_gate.py
  * 严格(可选,/claudee strict): PreToolUse 主动拦受控工具集 -> accept_gate.py
  * ASK: CC 多选一/计划批准等 -> 板提示"去终端选"(hooks 无法替你选)
"""

SLEEP = 0
STARTED = 1
WORKING = 2
IDLE = 3
NEEDPERM = 4
SUBAGENT = 5
DONE = 6
ASK = 7

CODE_MAX = ASK   # comm RX / clamp 合法上限

ACCEPT = b"A"
DENY = b"D"

# 严格模式"受控工具集"：只有这些会改动/执行的工具会被主动拦截放行。
DEFAULT_GATED = ("Bash", "Write", "Edit", "MultiEdit", "NotebookEdit")


def status_code_for(event, matcher=None):
    """把 CC hook 事件映射为状态字节；返回 int 或 None(不更新状态)。

    权限放行(NEEDPERM)由 permission_gate / 严格 accept_gate 驱动，不在此。"""
    if event == "UserPromptSubmit":
        return STARTED
    if event == "PostToolUse":
        return WORKING
    if event in ("Stop", "SubagentStop"):
        return IDLE
    if event == "SessionStart":
        return STARTED
    if event == "Notification":
        if matcher == "idle_prompt":
            return IDLE
        if matcher and "elicitation" in matcher:
            return ASK            # CC 在让你多选一
        # permission_prompt: 由 permission_gate 驱动 NEEDPERM，这里不重复
        return None
    return None


def is_gated(tool, gated=DEFAULT_GATED):
    """严格模式下该工具是否需要"挑战放行"。"""
    return tool in gated


def clamp_code(code):
    """把任意整数夹到合法状态字节，越界返回 None。"""
    try:
        code = int(code)
    except (TypeError, ValueError):
        return None
    return code if 0 <= code <= CODE_MAX else None


# ---- 帧：遥测(0xFE) / 情境(0xFC) ----
TELE_SOF = 0xFE
TELE_EOL = 0x0A
TELE_MAX = 31
CTX_SOF = 0xFC
CTX_EOL = 0x0A
CTX_MAX = 31


def _frame(sof, text, limit, eol):
    payload = str(text or "")[:limit].encode("ascii", "replace")
    return bytes([sof]) + payload + bytes([eol])


def telemetry_frame(text):
    """编码遥测帧 bytes：非 ASCII 用 '?'，payload 截断到 TELE_MAX。"""
    return _frame(TELE_SOF, text, TELE_MAX, TELE_EOL)


def context_frame(text):
    """编码情境帧 bytes（工具描述 / 问题），截断到 CTX_MAX。"""
    return _frame(CTX_SOF, text, CTX_MAX, CTX_EOL)


def humanize_tokens(n):
    """15500 -> '15.5k'; 800 -> '800'。"""
    try:
        n = int(n)
    except (TypeError, ValueError):
        return "0"
    if n >= 1000:
        return "%.1fk" % (n / 1000.0)
    return str(n)


def format_telemetry(model, cost_usd, tokens, pct):
    """组装板上遥测行，例: 'Opus $0.42 15.5k 8%' (<=TELE_MAX ASCII)。"""
    model = (str(model or "?").split() or ["?"])[0]   # "Opus 4.8" -> "Opus"
    try:
        cost = "$%.2f" % float(cost_usd or 0)
    except (TypeError, ValueError):
        cost = "$0.00"
    try:
        p = "%d%%" % int(float(pct or 0))
    except (TypeError, ValueError):
        p = "0%"
    return ("%s %s %s %s" % (model, cost, humanize_tokens(tokens), p))[:TELE_MAX]


def _basename(path):
    return str(path or "").replace("\\", "/").rstrip("/").split("/")[-1]


def tool_descriptor(tool, tool_input):
    """挑战屏顶上那行"CC 要干什么"，例: 'Bash: npm test' / 'Edit main.c'。<=CTX_MAX"""
    tool = str(tool or "?")
    ti = tool_input if isinstance(tool_input, dict) else {}
    detail = ""
    if tool == "Bash":
        detail = str(ti.get("command", "")).replace("\n", " ")
    elif tool in ("Edit", "Write", "MultiEdit"):
        detail = _basename(ti.get("file_path"))
    elif tool == "NotebookEdit":
        detail = _basename(ti.get("notebook_path"))
    elif tool in ("Read", "Grep", "Glob"):
        detail = _basename(ti.get("file_path") or ti.get("path")) or str(ti.get("pattern", ""))
    s = ("%s: %s" % (tool, detail)) if detail else tool
    return s[:CTX_MAX]


def pretool_decision(allow):
    """PreToolUse hook 输出（严格 accept_gate 用）—— 注意字段是 permissionDecision。"""
    return {"hookSpecificOutput": {
        "hookEventName": "PreToolUse",
        "permissionDecision": "allow" if allow else "deny",
        "permissionDecisionReason": "Claudee \U0001F980 " + ("approved" if allow else "denied"),
    }}


def permreq_decision(allow):
    """PermissionRequest hook 输出（忠实 permission_gate 用）—— 字段是 decision.behavior。"""
    return {"hookSpecificOutput": {
        "hookEventName": "PermissionRequest",
        "decision": {"behavior": "allow" if allow else "deny"},
    }}
