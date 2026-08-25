#!/usr/bin/env python3
import socket, sys, time

def send(cmd):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(3)
    s.connect("/tmp/nullos-mon.sock")
    s.sendall((cmd + "\n").encode())
    time.sleep(0.12)
    try:
        s.recv(65536)
    except socket.timeout:
        pass
    s.close()

KEYMAP = {
    " ": "spc", "/": "slash", "-": "minus", "=": "equal",
    ".": "dot", ",": "comma", ";": "semicolon", "'": "apostrophe",
    "[": "bracket_left", "]": "bracket_right", "\\": "backslash",
    "`": "grave_accent",
}

if __name__ == "__main__":
    cmd = sys.argv[1]
    if cmd == "shot":
        send("screendump /tmp/nullos-shot.ppm")
    elif cmd == "key":
        # one QEMU key name, e.g. "a", "ret", "spc", "shift"
        send("sendkey " + sys.argv[2])
    elif cmd == "text":
        # arbitrary string, expanded to per-character sendkeys
        for ch in sys.argv[2]:
            if ch.isupper():
                send("sendkey shift-" + ch.lower())
                continue
            name = KEYMAP.get(ch, ch)
            if len(name) > 1 and name not in (
                    "spc", "slash", "minus", "equal", "dot", "comma",
                    "semicolon", "apostrophe", "bracket_left",
                    "bracket_right", "backslash", "grave_accent"):
                continue  # unmapped punctuation: skip rather than misfire
            send("sendkey " + name)
    elif cmd == "q":
        send("quit")