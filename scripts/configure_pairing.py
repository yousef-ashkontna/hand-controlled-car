"""Create local ESP-NOW pairing configuration shared by both firmware builds."""

import argparse
from pathlib import Path
import re
import secrets


def mac_address(value):
    if not re.fullmatch(r"(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}", value):
        raise argparse.ArgumentTypeError("Use a station MAC such as 02:12:34:56:78:9A")
    result = bytes.fromhex(value.replace(":", ""))
    if not any(result) or result[0] & 1:
        raise argparse.ArgumentTypeError("A nonzero unicast station MAC is required")
    return result


def initializer(value):
    return "{" + ", ".join(f"0x{byte:02X}" for byte in value) + "}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sender", type=mac_address, required=True)
    parser.add_argument("--receiver", type=mac_address, required=True)
    parser.add_argument("--force", action="store_true", help="Rotate keys; both boards must be reflashed")
    args = parser.parse_args()
    if args.sender == args.receiver:
        parser.error("Sender and receiver must be different devices")
    output = Path(__file__).resolve().parents[1] / "include" / "pairing_config.h"
    if output.exists() and not args.force:
        parser.error("Pairing already exists; use --force only when reflashing both boards")
    contents = "// Local credentials. Do not commit or share.\n#pragma once\n"
    for name, value in (("SENDER_MAC", args.sender), ("RECEIVER_MAC", args.receiver),
                        ("PMK", secrets.token_bytes(16)), ("LMK", secrets.token_bytes(16))):
        contents += f"#define CAR_{name} {initializer(value)}\n"
    output.write_text(contents, encoding="utf-8")
    print("Pairing saved locally. Run 'pio run -t clean', then rebuild and flash BOTH boards. Keys were not printed.")


if __name__ == "__main__":
    main()
