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
# bluetoothctl may print the complete 48-byte value on one line.  Do not cap
# this at the conventional 16-byte hex-dump width.
HEX_LINE = re.compile(r"(?<![0-9a-fA-F])((?:[0-9a-fA-F]{2}(?:\s+|$))+)")
FRAME_SIZE = 48
FIELD_NAMES = (
    "positions", "layers", "modifiers", "indicators", "default-layer",
    "endpoint", "central-battery", "peripheral-battery", "split",
)
MODIFIER_NAMES = ("LCTL", "LSFT", "LALT", "LGUI", "RCTL", "RSFT", "RALT", "RGUI")


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
    if len(data) != FRAME_SIZE:
        return f"invalid record length {len(data)}: {data.hex(' ')}"
    if data[0] != 2:
        return f"unsupported protocol version {data[0]}: {data.hex(' ')}"
    declared_size = int.from_bytes(data[2:4], "little")
    if declared_size != FRAME_SIZE:
        return f"invalid declared frame size {declared_size}: {data.hex(' ')}"
    if data[1] & ~1:
        return f"unknown frame flags 0x{data[1]:02x}: {data.hex(' ')}"

    sequence = int.from_bytes(data[4:8], "little")
    timestamp = int.from_bytes(data[8:16], "little")
    positions = int.from_bytes(data[16:24], "little")
    layers = int.from_bytes(data[24:28], "little")
    changed = int.from_bytes(data[28:32], "little")
    valid = int.from_bytes(data[32:36], "little")
    modifiers = data[36]
    indicators = data[37]
    default_layer = data[38]
    transport_value = data[39]
    transport = {0: "unknown", 1: "USB", 2: "BLE"}.get(transport_value)
    profile = data[40]
    central_battery = data[41]
    peripheral_battery = data[42]
    split_value = data[43]
    split = {0: "unknown", 1: "down", 2: "up"}.get(split_value)
    dropped = int.from_bytes(data[44:48], "little")

    if transport is None:
        return f"invalid transport {transport_value}: {data.hex(' ')}"
    if split is None:
        return f"invalid split status {split_value}: {data.hex(' ')}"
    if valid & (1 << 4) and default_layer >= 32:
        return f"invalid default layer {default_layer}: {data.hex(' ')}"
    if valid & (1 << 5):
        if transport == "unknown":
            return f"valid endpoint has unknown transport: {data.hex(' ')}"
        if transport == "BLE" and profile == 0xff:
            return f"valid BLE endpoint has unknown profile: {data.hex(' ')}"
        if transport == "USB" and profile != 0xff:
            return f"valid USB endpoint has BLE profile {profile}: {data.hex(' ')}"
    if valid & (1 << 6) and central_battery > 100:
        return f"invalid central battery {central_battery}: {data.hex(' ')}"
    if valid & (1 << 7) and peripheral_battery > 100:
        return f"invalid peripheral battery {peripheral_battery}: {data.hex(' ')}"
    if valid & (1 << 8) and split == "unknown":
        return f"valid split status is unknown: {data.hex(' ')}"

    down = [str(i) for i in range(64) if positions & (1 << i)]
    mods = [name for bit, name in enumerate(MODIFIER_NAMES) if modifiers & (1 << bit)]
    changed_names = [name for bit, name in enumerate(FIELD_NAMES) if changed & (1 << bit)]
    valid_names = [name for bit, name in enumerate(FIELD_NAMES) if valid & (1 << bit)]
    kind = "snapshot" if data[1] & 1 else "state"

    optional = []
    if valid & (1 << 3):
        optional.append(f"leds=0x{indicators:02x}")
    if valid & (1 << 5):
        optional.append(f"output={transport}{profile if transport == 'BLE' else ''}")
    if valid & (1 << 6):
        optional.append(f"left-batt={central_battery}%")
    if valid & (1 << 7):
        optional.append(f"right-batt={peripheral_battery}%")
    if valid & (1 << 8):
        optional.append(f"split={split}")

    return (
        f"seq={sequence:10} t={timestamp:12}ms {kind:8} "
        f"changed=[{','.join(changed_names)}] valid=[{','.join(valid_names)}] "
        f"layers=0x{layers:08x} "
        f"default={default_layer if valid & (1 << 4) else '?'} keys=[{','.join(down)}] "
        f"mods=[{','.join(mods)}] drops={dropped} {' '.join(optional)}"
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
                if len(pending) >= FRAME_SIZE:
                    print(decode(bytes(pending[:FRAME_SIZE])), flush=True)
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
