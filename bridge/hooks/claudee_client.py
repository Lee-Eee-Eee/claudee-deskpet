"""
claudee_client.py  —  hook 共用：激活标记 + 连桥发 JSON
放在 hooks/ 目录，被 status_out / accept_gate / claudee_ctl 复用。
"""
import json
import os
import socket

MARKER = os.path.expanduser("~/.claude/claudee/active")
STRICT_MARKER = os.path.expanduser("~/.claude/claudee/strict")


def is_active():
    return os.path.exists(MARKER)


def set_active(on, session=None):
    d = os.path.dirname(MARKER)
    os.makedirs(d, exist_ok=True)
    if on:
        with open(MARKER, "w") as f:
            f.write(session or "")
    else:
        try:
            os.remove(MARKER)
        except FileNotFoundError:
            pass


def is_strict():
    """严格/游戏化模式：PreToolUse 主动拦受控工具(常出挑战)。默认关。"""
    return os.path.exists(STRICT_MARKER)


def set_strict(on):
    d = os.path.dirname(STRICT_MARKER)
    os.makedirs(d, exist_ok=True)
    if on:
        open(STRICT_MARKER, "w").close()
    else:
        try:
            os.remove(STRICT_MARKER)
        except FileNotFoundError:
            pass


def _resolv_host():
    """WSL2 NAT 模式下，Windows 主机 IP 常见来源之一 = /etc/resolv.conf 的 nameserver。"""
    try:
        with open("/etc/resolv.conf") as f:
            for line in f:
                if line.startswith("nameserver"):
                    return line.split()[1]
    except OSError:
        pass
    return None


def _gateway_host():
    """WSL2 NAT 模式下，默认网关常即 Windows 主机（vEthernet）。"""
    try:
        import subprocess
        out = subprocess.check_output(["ip", "route", "show", "default"],
                                      timeout=1.0).decode()
        parts = out.split()
        if "via" in parts:
            return parts[parts.index("via") + 1]
    except Exception:
        pass
    return None


def _candidates():
    """要尝试的 (host,port)：env(CLAUDEE_BRIDGE) 优先；其次 localhost(镜像网络)；
    再次默认网关 / resolv nameserver(NAT 下的 Windows 主机)。"""
    port = int(os.environ.get("CLAUDEE_PORT", "8787"))
    env = os.environ.get("CLAUDEE_BRIDGE")
    if env:
        host, _, p = env.partition(":")
        yield (host or "127.0.0.1", int(p or port))
        return
    seen = set()
    for h in ("127.0.0.1", _gateway_host(), _resolv_host()):
        if h and h not in seen:
            seen.add(h)
            yield (h, port)


def send(obj, expect_reply=False, connect_timeout=2.0, read_timeout=5.0):
    """连桥发一行 JSON。expect_reply=True 时读一行 JSON 回复。
    失败返回 {"ok":False,"error":...}（hook 据此安全回落）。"""
    last = None
    for host, port in _candidates():
        try:
            with socket.create_connection((host, port), timeout=connect_timeout) as s:
                s.sendall((json.dumps(obj) + "\n").encode("utf-8"))
                if not expect_reply:
                    return {"ok": True}
                s.settimeout(read_timeout)
                data = b""
                while b"\n" not in data:
                    chunk = s.recv(4096)
                    if not chunk:
                        break
                    data += chunk
                return json.loads(data.decode("utf-8").strip() or "{}")
        except Exception as e:
            last = e
            continue
    return {"ok": False, "error": str(last)}
