#!/usr/bin/env python3
"""Reproduce post-hook startup against the user's verified retail EXE, read-only.
The EXE is never run, modified, or bundled. A temporary byte buffer is patched
by the actual production crouch patch, then checked by the production gates.
Needs a C++17 compiler; does not build the Windows DLL.
"""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'tools'))
from verify_retail_profile import verify

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('exe', type=Path)
args = parser.parse_args()
data = args.exe.read_bytes()
verify(data, json.loads((root / 'research/retail-1.10-profile.json').read_text()))
pe = struct.unpack_from('<I', data, 0x3c)[0]
optional = pe + 24
image = bytearray(0x491000)
sections = optional + struct.unpack_from('<H', data, pe + 20)[0]
for i in range(struct.unpack_from('<H', data, pe + 6)[0]):
    rva, size, offset = struct.unpack_from('<III', data, sections + i*40 + 12)
    image[rva:rva+size] = data[offset:offset+size]
native = (root / 'HaloCEVR/Immersion/RetailWeaponEngine.cpp').read_text()
hooks = (root / 'HaloCEVR/Hooking/Hooks.cpp').read_text()
source = (root / 'tests/runtime_profile_fixture.cpp').read_text()
source = source.replace('// INSERT_MATCH', native[native.index('bool MatchFingerprint('):native.index('// 0x4d6fc0:')])
source = source.replace('// INSERT_GATE', native[native.index('void RetailWeaponEngine::CheckInteractionCode()'):native.index('bool RetailWeaponEngine::Initialize()')])
source = source.replace('// INSERT_CROUCH_PATCH', hooks[hooks.index('void Hooks::P_DisableCrouchCamera()'):hooks.index('void Hooks::P_ForceCmdLineArgs()')])
with tempfile.TemporaryDirectory(prefix='halo-runtime-profile-') as temp:
    folder = Path(temp)
    (folder / 'image.bin').write_bytes(image)
    (folder / 'fixture.cpp').write_text(source)
    binary = folder / ('profile.exe' if os.name == 'nt' else 'profile')
    subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=undefined', '-I', str(root), str(folder/'fixture.cpp'), '-o', str(binary)], check=True)
    subprocess.run([str(binary), str(folder/'image.bin')], check=True)
