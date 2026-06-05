#!/usr/bin/env python3
"""
bridge.py  —  Claudee PC 端桥接（跑在 *Windows* 上，独占 COM 口）

职责:
  * 打开并独占串口 (默认 9600 8N1) 连到板子 UART1 (PTC3/PTC4)
  * 本地 TCP 服务，供 WSL 里的 CC hook 连接
  * 状态码 -> 串口字节; 心跳每秒重发当前状态(保活+同步)
  * 受控工具放行: 收 wait_accept -> 让板子进 NEEDPERM -> 阻塞等板子回 'A'/'D'/超时

TCP 行分隔 JSON 指令:
  {"cmd":"status","code":N}          上报状态(N=0..6)
  {"cmd":"activate","session":"..."} /claudee 激活
  {"cmd":"deactivate"}               /claudee off
  {"cmd":"telemetry","text":"..."}   statusLine 上报 token/cost/ctx -> 板底显示
  {"cmd":"context","text":"..."}     挑战工具描述 / ASK 问题 -> 板挑战/ASK 屏显示
  {"cmd":"wait_accept","timeout":S}  受控工具阻塞放行 -> 回 {"decision":"allow|deny|ask"}
  {"cmd":"inject","byte":"A"}        (调试)无板子时模拟板子回传，便于联调
  {"cmd":"ping"}                     -> {"ok":true,"active":bool,"code":N}

用法 (Windows):  python bridge.py --com COM5
     (无硬件联调):python bridge.py --dry-run
"""
import argparse
import json
import os
import queue
import socket
import socketserver
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import protocol as P


class DummySerial:
    """无串口时的占位：写=打印，读=空。用于 --dry-run 联调。"""

    def __init__(self):
        self._log = True

    def write(self, data):
        data = bytes(data)
        if data and data[0] == P.TELE_SOF:        # 遥测帧
            txt = data[1:].split(b"\n", 1)[0].decode("ascii", "replace")
            print("[serial->board] TELE: %s" % txt, flush=True)
            return
        if data and data[0] == P.CTX_SOF:         # 情境帧(工具描述/问题)
            txt = data[1:].split(b"\n", 1)[0].decode("ascii", "replace")
            print("[serial->board] CTX: %s" % txt, flush=True)
            return
        names = {0: "SLEEP", 1: "STARTED", 2: "WORKING", 3: "IDLE",
                 4: "NEEDPERM", 5: "SUBAGENT", 6: "DONE", 7: "ASK"}
        for b in data:
            print("[serial->board] 0x%02X %s" % (b, names.get(b, "")), flush=True)

    def read(self, n=1):
        time.sleep(0.2)
        return b""

    def close(self):
        pass


class Bridge:
    def __init__(self, ser, accept_timeout=300.0, heartbeat=1.0):
        self.ser = ser
        self.accept_timeout = accept_timeout
        self.heartbeat = heartbeat
        self.state_lock = threading.Lock()
        self.ser_lock = threading.Lock()
        self.active = False
        self.session = None
        self.code = P.SLEEP
        self.telemetry = ""          # 最近一条 token/cost/ctx 遥测行
        self.accept_q = queue.Queue()
        self._stop = threading.Event()

    # ---- 串口写 ----
    def _write(self, code):
        with self.ser_lock:
            try:
                self.ser.write(bytes([code & 0xFF]))
            except Exception as e:  # 串口异常不致命
                print("[bridge] serial write error:", e, flush=True)

    def _write_frame(self, text):
        with self.ser_lock:
            try:
                self.ser.write(P.telemetry_frame(text))
            except Exception as e:
                print("[bridge] serial frame error:", e, flush=True)

    def _write_ctx(self, text):
        with self.ser_lock:
            try:
                self.ser.write(P.context_frame(text))
            except Exception as e:
                print("[bridge] serial ctx error:", e, flush=True)

    def set_telemetry(self, text):
        """statusLine -> 板：token/cost/ctx 行。仅激活时下发。"""
        text = str(text or "")[:P.TELE_MAX]
        with self.state_lock:
            self.telemetry = text
            active = self.active
        if active:
            self._write_frame(text)

    def set_context(self, text):
        """情境行 -> 板：挑战时的工具描述 / ASK 的问题。仅激活时下发。"""
        with self.state_lock:
            active = self.active
        if active:
            self._write_ctx(str(text or "")[:P.CTX_MAX])

    def set_code(self, code, send=True):
        code = P.clamp_code(code)
        if code is None:
            return
        with self.state_lock:
            self.code = code
        if send:
            self._write(code)

    def activate(self, session=None):
        with self.state_lock:
            self.active = True
            self.session = session
            self.code = P.IDLE          # 唤醒：在线但空闲
        self._write(P.IDLE)
        print("[bridge] ACTIVATED session=%s" % session, flush=True)

    def deactivate(self):
        with self.state_lock:
            self.active = False
            self.code = P.SLEEP
        self._write(P.SLEEP)
        print("[bridge] DEACTIVATED -> SLEEP", flush=True)

    def wait_accept(self, timeout=None):
        """让板子进 NEEDPERM, 阻塞等 'A'/'D'。返回 'allow'/'deny'/'ask'。"""
        with self.state_lock:
            active = self.active
        if not active:
            return "ask"                # 未激活: 不接管, 回落 CC 正常询问
        # 清空陈旧回传
        try:
            while True:
                self.accept_q.get_nowait()
        except queue.Empty:
            pass
        self.set_code(P.NEEDPERM)
        t = self.accept_timeout if timeout is None else float(timeout)
        try:
            b = self.accept_q.get(timeout=t)
        except queue.Empty:
            self.set_code(P.WORKING)    # 超时: 回工作态, CC 回落正常询问
            return "ask"
        self.set_code(P.WORKING)        # 放行后 CC 继续 -> 工作态
        return "allow" if b == P.ACCEPT else "deny"

    # ---- 串口读线程 ----
    def reader_loop(self):
        while not self._stop.is_set():
            try:
                b = self.ser.read(1)
            except Exception as e:
                print("[bridge] serial read error:", e, flush=True)
                time.sleep(0.5)
                continue
            if b in (P.ACCEPT, P.DENY):
                self.accept_q.put(b)
                print("[bridge] board -> %s" % b.decode(), flush=True)

    # ---- 心跳线程: 保活 + 同步 ----
    def heartbeat_loop(self):
        n = 0
        while not self._stop.is_set():
            with self.state_lock:
                active = self.active
                code = self.code if active else P.SLEEP
                tele = self.telemetry
            self._write(code)
            # 每 ~5 个心跳重发一次遥测：板子断电重连/上电后能追上
            if active and tele and (n % 5 == 0):
                self._write_frame(tele)
            n += 1
            self._stop.wait(self.heartbeat)

    def stop(self):
        self._stop.set()


def make_handler(bridge):
    class Handler(socketserver.StreamRequestHandler):
        def handle(self):
            try:
                line = self.rfile.readline()
                if not line:
                    return
                msg = json.loads(line.decode("utf-8").strip() or "{}")
            except Exception as e:
                self._reply({"ok": False, "error": "bad json: %s" % e})
                return
            cmd = msg.get("cmd")
            if cmd == "status":
                bridge.set_code(msg.get("code"))
                self._reply({"ok": True})
            elif cmd == "activate":
                bridge.activate(msg.get("session"))
                self._reply({"ok": True})
            elif cmd == "deactivate":
                bridge.deactivate()
                self._reply({"ok": True})
            elif cmd == "telemetry":
                bridge.set_telemetry(msg.get("text"))
                self._reply({"ok": True})
            elif cmd == "context":
                bridge.set_context(msg.get("text"))
                self._reply({"ok": True})
            elif cmd == "wait_accept":
                decision = bridge.wait_accept(msg.get("timeout"))
                self._reply({"ok": True, "decision": decision})
            elif cmd == "inject":     # 调试: 模拟板子回传
                b = (msg.get("byte") or "A").encode()[:1]
                bridge.accept_q.put(P.ACCEPT if b == P.ACCEPT else P.DENY)
                self._reply({"ok": True})
            elif cmd == "ping":
                with bridge.state_lock:
                    self._reply({"ok": True, "active": bridge.active, "code": bridge.code})
            else:
                self._reply({"ok": False, "error": "unknown cmd"})

        def _reply(self, obj):
            try:
                self.wfile.write((json.dumps(obj) + "\n").encode("utf-8"))
            except Exception:
                pass
    return Handler


class ThreadingTCPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    allow_reuse_address = True
    daemon_threads = True


def open_serial(args):
    if args.dry_run or not args.com:
        print("[bridge] DRY-RUN (no serial). use --com COMx for real board.", flush=True)
        return DummySerial()
    try:
        import serial  # pyserial
    except ImportError:
        print("ERROR: pyserial not installed. Run:  pip install pyserial", file=sys.stderr)
        sys.exit(1)
    s = serial.Serial(args.com, args.baud, timeout=0.2)
    print("[bridge] serial open: %s @ %d" % (args.com, args.baud), flush=True)
    return s


def main():
    ap = argparse.ArgumentParser(description="Claudee PC bridge")
    ap.add_argument("--com", "-p", default=os.environ.get("CLAUDEE_COM"),
                    help="serial port, e.g. COM5 (Windows) or /dev/ttyACM0")
    ap.add_argument("--baud", type=int, default=9600)
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--port", type=int, default=int(os.environ.get("CLAUDEE_PORT", "8787")))
    ap.add_argument("--accept-timeout", type=float, default=300.0)
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    ser = open_serial(args)
    bridge = Bridge(ser, accept_timeout=args.accept_timeout)

    threading.Thread(target=bridge.reader_loop, daemon=True).start()
    threading.Thread(target=bridge.heartbeat_loop, daemon=True).start()

    srv = ThreadingTCPServer((args.host, args.port), make_handler(bridge))
    print("[bridge] listening on %s:%d  (accept timeout %.0fs)"
          % (args.host, args.port, args.accept_timeout), flush=True)
    bridge._write(P.SLEEP)  # 开机先让板子睡
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        bridge.stop()
        srv.shutdown()
        try:
            ser.close()
        except Exception:
            pass


if __name__ == "__main__":
    main()
