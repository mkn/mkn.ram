#! /usr/bin/env python3
"""
mkn.manifest.yml for a mkn.ram release, see .github/workflows/release.yml

    manifest.py collect <out dir> <key> <mkn version> <profile> <mode> <bin dir>
        copies the libraries one build left in <bin dir> into <out dir> with short names, and adds them
        with their sha256 to <out dir>/<os>-<arch>.json under "<key>;profile=<profile>;mode=<mode>"
    manifest.py merge <out file> <version> <commit> <fragment.json>...
        mkn.manifest.yml from every build's fragment

A key names what a binary needs to match, e.g. "os=nix;arch=x86_64;cxxrt=libstdc++;libc=manylinux_2_28".
maiken adds ";profile=<profile, @ for the top level>;mode=<static|shared>" to it.
"""

import hashlib
import json
import os
import shutil
import sys

NAME = "mkn.ram"
LIBS = (".a", ".so", ".dylib", ".lib", ".dll")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def collect(out, key, mkn, profile, mode, bin_dir):
    fields = dict(f.split("=", 1) for f in key.split(";"))
    stem = f"{NAME}-{fields['os']}-{fields['arch']}-{'default' if profile == '@' else profile}-{mode}"
    os.makedirs(out, exist_ok=True)
    files = []
    for f in sorted(os.listdir(bin_dir)):
        ext = os.path.splitext(f)[1]
        if ext not in LIBS or not os.path.isfile(os.path.join(bin_dir, f)):
            continue
        name = stem + ext
        shutil.copyfile(os.path.join(bin_dir, f), os.path.join(out, name))
        files.append({"file": name, "sha256": sha256(os.path.join(out, name))})
    if not files:
        sys.exit(f"no libraries in {bin_dir}")
    fragment_path = os.path.join(out, f"{fields['os']}-{fields['arch']}.json")
    fragment = {"mkn": mkn, "bins": {}}
    if os.path.exists(fragment_path):
        with open(fragment_path) as f:
            fragment = json.load(f)
    fragment["bins"][f"{key};profile={profile};mode={mode}"] = files
    with open(fragment_path, "w") as f:
        json.dump(fragment, f, indent=2)


def merge(out, version, commit, fragments):
    lines = ["format: 1", f"name: {NAME}", f"version: {version}", f"commit: {commit}", "bins:"]
    for path in fragments:
        with open(path) as f:
            fragment = json.load(f)
        for key, files in fragment["bins"].items():
            lines.append(f'  "{key}":')
            lines.append(f"    mkn: {fragment['mkn']}")
            lines.append("    files:")
            for file in files:
                lines.append(f"    - file: {file['file']}")
                lines.append(f"      sha256: {file['sha256']}")
    with open(out, "w") as f:
        f.write("\n".join(lines) + "\n")


def main():
    if len(sys.argv) == 8 and sys.argv[1] == "collect":
        collect(*sys.argv[2:])
    elif len(sys.argv) >= 6 and sys.argv[1] == "merge":
        merge(sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5:])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
