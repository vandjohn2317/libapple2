#!/usr/bin/env python3
"""Wrap the code binary in a .tap with a BASIC loader.

    10 CLEAR 32767: LOAD "" CODE: RANDOMIZE USR 32768
"""
import struct, sys


def block(flag, data):
    body = bytes([flag]) + data
    chk = 0
    for b in body:
        chk ^= b
    body += bytes([chk])
    return struct.pack("<H", len(body)) + body


def basic_number(n):
    return bytes([0x0E, 0, 0]) + struct.pack("<H", n) + bytes([0])


def basic_loader(name, org):
    line = (bytes([0xFD]) + b"32767" + basic_number(32767) + b":"
            + bytes([0xEF]) + b'""' + bytes([0xAF]) + b":"
            + bytes([0xF9, 0xC0]) + str(org).encode() + basic_number(org) + b"\r")
    prog = struct.pack(">H", 10) + struct.pack("<H", len(line)) + line
    hdr = bytes([0]) + name.ljust(10)[:10].encode() + struct.pack("<HHH", len(prog), 10, len(prog))
    return block(0, hdr) + block(0xFF, prog)


def code_block(name, org, data):
    hdr = bytes([3]) + name.ljust(10)[:10].encode() + struct.pack("<HHH", len(data), org, 32768)
    return block(0, hdr) + block(0xFF, data)


def main():
    src, dst = sys.argv[1], sys.argv[2]
    org = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x8000
    data = open(src, "rb").read()
    tap = basic_loader("orbs", org) + code_block("orbscode", org, data)
    open(dst, "wb").write(tap)
    print(f"{dst}: {len(data)} bytes of code at 0x{org:04X}, ends 0x{org + len(data) - 1:04X}")


if __name__ == "__main__":
    main()
