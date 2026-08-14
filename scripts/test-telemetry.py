#!/usr/bin/env python3
"""Subscribe to Chocofi ZMK telemetry with BlueZ's bluetoothctl."""

import os
import pty
import re
import select
import shutil
import signal
import subprocess
import sys
import time

CHAR_UUID = "9e7a7d70-df1b-4f76-9d45-8c3f4a6b2101"
ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
# bluetoothctl may print the complete 20-byte value on one line.  Do not cap
# this at the conventional 16-byte hex-dump width.
HEX_LINE = re.compile(r"(?<![0-9a-fA-F])((?:[0-9a-fA-F]{2}(?:\s+|$))+)")
TYPES = {1: "snapshot", 2: "key", 3: "layers"}


def find_keyboard():
    for command in (["bluetoothctl", "devices", "Paired"],
                    ["bluetoothctl", "paired-devices"]):
        result = subprocess.run(command, text=True, capture_output=True)
        matches = re.findall(
            r"Device\s+([0-9A-Fa-f:]{17})\s+.*Chocochap.*", result.stdout
        )
        if len(matches) == 1:
            return matches[0]
        if len(matches) > 1:
            sys.exit("More than one paired Chocochap found; pass its MAC address.")
    sys.exit("No paired Chocochap found; pair it first or pass its MAC address.")


def decode(data):
    if len(data) != 20:
        return f"invalid record length {len(data)}: {data.hex(' ')}"
    if data[0] != 1:
        return f"unsupported protocol version {data[0]}: {data.hex(' ')}"

    kind = TYPES.get(data[1], f"unknown({data[1]})")
    pressed = bool(data[2] & 1)
    position = None if data[3] == 0xFF else data[3]
    sequence = int.from_bytes(data[4:6], "little")
    timestamp = int.from_bytes(data[6:10], "little")
    layers = int.from_bytes(data[10:14], "little")
    positions = int.from_bytes(data[14:20], "little")
    down = [str(i) for i in range(48) if positions & (1 << i)]

    event = ""
    if kind == "key":
        event = f" pos={position} {'DOWN' if pressed else 'UP'}"
    return (
        f"seq={sequence:5} t={timestamp:10}ms {kind:8}{event} "
        f"layers=0x{layers:08x} keys=[{','.join(down)}]"
    )


def main():
    if not shutil.which("bluetoothctl"):
        sys.exit("bluetoothctl not found; install/enable BlueZ first.")

    address = sys.argv[1] if len(sys.argv) > 1 else find_keyboard()
    if not re.fullmatch(r"[0-9A-Fa-f:]{17}", address):
        sys.exit(f"Usage: {sys.argv[0]} [XX:XX:XX:XX:XX:XX]")

    print(f"Connecting to {address}...")
    subprocess.run(["bluetoothctl", "connect", address], timeout=20, check=False)

    master, slave = pty.openpty()
    process = subprocess.Popen(
        ["bluetoothctl"], stdin=slave, stdout=slave, stderr=slave, close_fds=True
    )
    os.close(slave)

    def send(command, delay=1.0):
        os.write(master, (command + "\n").encode())
        time.sleep(delay)

    try:
        time.sleep(1)
        send("menu gatt")
        send(f"list-attributes {address}", 2)
        send(f"select-attribute {CHAR_UUID}")
        send("notify on", 2)
        print("\nSubscription requested. Press keys on both halves; Ctrl-C stops.\n")

        pending = bytearray()
        collecting = False
        text_buffer = ""
        first_record = False
        first_record_deadline = time.monotonic() + 10
        while process.poll() is None:
            ready, _, _ = select.select([master], [], [], 1)
            if not ready:
                if not first_record and time.monotonic() >= first_record_deadline:
                    print(
                        "No telemetry received. Check the bluetoothctl errors above; "
                        "an old BlueZ GATT cache may require removing and re-pairing the keyboard.",
                        file=sys.stderr,
                    )
                    first_record = True
                continue
            try:
                # readline redraws the prompt with carriage returns when an
                # asynchronous GATT notification arrives. Treat those as line
                # boundaries; deleting them can join the prompt to a hex row.
                text_buffer += os.read(master, 4096).decode(errors="replace").replace("\r", "\n")
            except OSError:
                break

            while "\n" in text_buffer:
                line, text_buffer = text_buffer.split("\n", 1)
                line = ANSI.sub("", line)
                lower = line.lower()
                if any(word in lower for word in ("failed", "not available", "not found")):
                    print(f"bluetoothctl: {line.strip()}", file=sys.stderr)
                if "attribute " in lower and " value:" in lower:
                    pending.clear()
                    collecting = True
                    continue
                if not collecting:
                    continue
                match = HEX_LINE.search(line)
                if not match:
                    continue
                pending.extend(int(value, 16) for value in match.group(1).split())
                if len(pending) >= 20:
                    print(decode(bytes(pending[:20])), flush=True)
                    first_record = True
                    pending.clear()
                    collecting = False
    except KeyboardInterrupt:
        pass
    finally:
        if process.poll() is None:
            try:
                send("notify off", 0.2)
                send("quit", 0.2)
            except OSError:
                pass
            process.send_signal(signal.SIGTERM)
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        os.close(master)


if __name__ == "__main__":
    main()
