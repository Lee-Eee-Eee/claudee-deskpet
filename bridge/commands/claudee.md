---
description: Toggle the Claudee desk-pet (🦀 Clawd) for this Claude Code session
argument-hint: [on|off|strict|status]
allowed-tools: Bash(python3 *)
---

Use the **Bash tool** to run this command (append the argument I gave, or `on` if I gave none), then report its output to me — do nothing else:

```bash
python3 ~/.claude/claudee/hooks/claudee_ctl.py on
```

If I typed an argument (`off`, `strict`, `status`), use that instead of `on`.

While armed, the board crab (Clawd) mirrors my live status; when I request a gated tool you approve it by finishing a squat or mini-game challenge on the board. `/claudee off` puts the crab to sleep; `/claudee strict` turns on the gate-everything game mode.
