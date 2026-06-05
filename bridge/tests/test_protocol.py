import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import protocol as P


def test_status_simple():
    assert P.status_code_for("UserPromptSubmit") == P.STARTED
    assert P.status_code_for("PostToolUse") == P.WORKING
    assert P.status_code_for("Stop") == P.IDLE
    assert P.status_code_for("SubagentStop") == P.IDLE
    assert P.status_code_for("SessionStart") == P.STARTED


def test_notification():
    assert P.status_code_for("Notification", "idle_prompt") == P.IDLE
    assert P.status_code_for("Notification", "elicitation_dialog") == P.ASK
    # permission_prompt 由 permission_gate 驱动 NEEDPERM，这里不重复
    assert P.status_code_for("Notification", "permission_prompt") is None
    assert P.status_code_for("Notification") is None


def test_pretooluse_not_mapped_here():
    # PreToolUse 的状态/放行由 accept_gate 处理，不在 status 映射里
    assert P.status_code_for("PreToolUse") is None
    assert P.status_code_for("Whatever") is None


def test_gated():
    assert P.is_gated("Bash")
    assert P.is_gated("Edit")
    assert P.is_gated("Write")
    assert P.is_gated("NotebookEdit")
    assert not P.is_gated("Read")
    assert not P.is_gated("Grep")
    assert not P.is_gated("Glob")


def test_clamp_code():
    assert P.clamp_code(0) == 0
    assert P.clamp_code(6) == 6
    assert P.clamp_code(P.NEEDPERM) == 4
    assert P.clamp_code(7) == P.ASK          # ASK 现在是合法状态
    assert P.clamp_code(8) is None
    assert P.clamp_code(-1) is None
    assert P.clamp_code("x") is None
    assert P.clamp_code(None) is None


def test_codes_match_firmware():
    # 必须与 Claudee/Sources/claudee.h 的 cc_state_t 一致
    assert (P.SLEEP, P.STARTED, P.WORKING, P.IDLE, P.NEEDPERM, P.SUBAGENT, P.DONE, P.ASK) \
        == (0, 1, 2, 3, 4, 5, 6, 7)


def test_humanize_tokens():
    assert P.humanize_tokens(0) == "0"
    assert P.humanize_tokens(800) == "800"
    assert P.humanize_tokens(15500) == "15.5k"
    assert P.humanize_tokens(1000000) == "1000.0k"
    assert P.humanize_tokens(None) == "0"
    assert P.humanize_tokens("x") == "0"


def test_format_telemetry():
    # "Opus 4.8" -> "Opus"; 累计花费; token 人性化; 百分比取整
    assert P.format_telemetry("Opus 4.8", 0.4231, 18743, 8.6) == "Opus $0.42 18.7k 8%"
    assert P.format_telemetry("Opus", 0, 0, 0) == "Opus $0.00 0 0%"
    # 容错：None / 坏值不抛
    out = P.format_telemetry(None, None, None, None)
    assert out.startswith("? $0.00 0 0%")
    # 长度封顶
    assert len(P.format_telemetry("VeryLongModelNameHere", 12345.6, 999999, 100)) <= P.TELE_MAX


def test_telemetry_frame():
    fr = P.telemetry_frame("Opus $0.42 18.7k 8%")
    assert fr[0] == P.TELE_SOF == 0xFE
    assert fr[-1] == P.TELE_EOL == 0x0A
    assert fr[1:-1] == b"Opus $0.42 18.7k 8%"
    # 截断到 TELE_MAX + 非 ASCII 替换
    long = P.telemetry_frame("x" * 100)
    assert len(long) == P.TELE_MAX + 2          # SOF + payload + EOL
    assert P.telemetry_frame("naïve")[1:-1] == b"na?ve"


def test_context_frame():
    fr = P.context_frame("Bash: npm test")
    assert fr[0] == P.CTX_SOF == 0xFC
    assert fr[-1] == P.CTX_EOL == 0x0A
    assert fr[1:-1] == b"Bash: npm test"
    assert len(P.context_frame("y" * 100)) == P.CTX_MAX + 2
    # SOF 不与状态字节/遥测 SOF 冲突
    assert P.CTX_SOF not in (P.TELE_SOF,) and P.CTX_SOF > P.CODE_MAX


def test_tool_descriptor():
    assert P.tool_descriptor("Bash", {"command": "npm test"}) == "Bash: npm test"
    assert P.tool_descriptor("Edit", {"file_path": "/a/b/main.c"}) == "Edit: main.c"
    assert P.tool_descriptor("Write", {"file_path": "C:\\x\\y\\app.py"}) == "Write: app.py"
    assert P.tool_descriptor("Read", {"file_path": "/p/q/r.txt"}) == "Read: r.txt"
    assert P.tool_descriptor("Grep", {"pattern": "foo"}) == "Grep: foo"
    assert P.tool_descriptor("ExitPlanMode", None) == "ExitPlanMode"
    assert len(P.tool_descriptor("Bash", {"command": "x" * 200})) <= P.CTX_MAX
    # 换行被压平
    assert "\n" not in P.tool_descriptor("Bash", {"command": "a\nb"})


def test_decisions():
    a = P.pretool_decision(True)
    assert a["hookSpecificOutput"]["hookEventName"] == "PreToolUse"
    assert a["hookSpecificOutput"]["permissionDecision"] == "allow"
    assert P.pretool_decision(False)["hookSpecificOutput"]["permissionDecision"] == "deny"
    b = P.permreq_decision(True)
    assert b["hookSpecificOutput"]["hookEventName"] == "PermissionRequest"
    assert b["hookSpecificOutput"]["decision"]["behavior"] == "allow"
    assert P.permreq_decision(False)["hookSpecificOutput"]["decision"]["behavior"] == "deny"
