#!/usr/bin/env python3
"""Read-only check of the executable used for the retail inventory investigation.

No packages, executable loading, disassembler or game installation required.
This checks the ON-DISK image, not memory after HaloCEVR/Chimera install hooks.
An exact match does not establish that an untested native call is safe in-game.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def verify(data, profile):
    def read(fmt, offset):
        size = struct.calcsize(fmt)
        if offset < 0 or offset + size > len(data):
            raise ValueError("Truncated PE structure")
        return struct.unpack_from(fmt, data, offset)[0]

    if data[:2] != b"MZ":
        raise ValueError("Not a Windows executable")
    pe = read("<I", 0x3C)
    if data[pe:pe+4] != b"PE\0\0":
        raise ValueError("Invalid PE signature")
    optional_size = read("<H", pe+20)
    optional = pe+24
    if optional_size < 96 or read("<H", optional) != 0x10B:
        raise ValueError("Expected a PE32 executable")
    headers = {
        "machine": read("<H", pe+4),
        "timestamp": read("<I", pe+8),
        "image_base": read("<I", optional+28),
        "image_size": read("<I", optional+56),
    }
    if headers != profile["pe"]:
        raise ValueError("Executable headers do not match the verified retail build")
    section_count = read("<H", pe+6)
    if not 1 <= section_count <= 96:
        raise ValueError("Invalid PE section count")
    sections = []
    for i in range(section_count):
        section = optional + optional_size + i*40
        sections.append((read("<I", section+12), read("<I", section+16),
                         read("<I", section+20), read("<I", section+36)))

    checked = []
    for routine in profile["routines"]:
        rva, size = routine["rva"], routine["length"]
        matches = [(offset+rva-base, flags) for base, raw_size, offset, flags in sections
                   if size > 0 and base <= rva and rva+size <= base+raw_size]
        if len(matches) != 1 or not matches[0][1] & 0x20000000:
            raise ValueError(f"{routine['name']}: no unique executable section")
        offset = matches[0][0]
        raw = data[offset:offset+size]
        if len(raw) != size or hashlib.sha256(raw).hexdigest() != routine["sha256"]:
            raise ValueError(f"{routine['name']}: instruction bytes do not match")
        if "fnv1a" in routine:
            fnv = 2166136261
            for byte in raw:
                fnv = ((fnv ^ byte) * 16777619) & 0xffffffff
            if fnv != routine["fnv1a"]:
                raise ValueError(f"{routine['name']}: runtime fingerprint differs")
        for variant in routine.get("runtime_variants", []):
            patched = bytearray(raw)
            begin = variant["offset"]
            replacement = bytes.fromhex(variant["bytes"])
            if begin < 0 or begin + len(replacement) > len(patched):
                raise ValueError("Runtime variant outside routine")
            patched[begin:begin+len(replacement)] = replacement
            fnv = 2166136261
            for byte in patched:
                fnv = ((fnv ^ byte) * 16777619) & 0xffffffff
            if fnv != variant["fnv1a"]:
                raise ValueError("Runtime variant fingerprint differs")
        checked.append(routine["name"])

    digest = hashlib.sha256(data).hexdigest()
    if digest != profile["sha256"]:
        raise ValueError("Function bytes match, but the full executable hash differs")
    return {"edition": profile["edition"], "version": profile["version"],
            "sha256": digest, "checked_routines": checked,
            "status": "Exact on-disk match; runtime behavior remains unverified"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    profile = json.loads((root / "research/retail-1.10-profile.json").read_text())
    try:
        header = (root / "HaloCEVR/Immersion/RetailProfile.h").read_text()
        compiled = [(int(rva,16), int(size,16), int(fnv,16)) for rva,size,fnv in
                    re.findall(r"\{0x([0-9A-F]+)u, 0x([0-9A-F]+)u, 0x([0-9A-F]+)u\}",header)]
        expected = [(r["rva"],r["length"],r["fnv1a"]) for r in profile["routines"] if "fnv1a" in r]
        if not compiled or compiled != expected:
            raise ValueError("Runtime C++ fingerprints differ from the research profile")
        variants = [v for routine in profile["routines"] for v in routine.get("runtime_variants", [])]
        declared = re.search(r"RetailSeatCameraCrouchDisabledFNV = 0x([0-9A-F]+)u", header)
        if len(variants) != 1 or declared is None or int(declared[1], 16) != variants[0]["fnv1a"]:
            raise ValueError("Runtime crouch-patch variant differs from research profile")
        result = verify(args.exe.read_bytes(), profile)
    except (ValueError, OSError) as error:
        parser.exit(1, f"Profile rejected: {error}\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
