"""Build lid-classpassives.dll, and with --test run the tests against it.

GitHub builds every release with this same script, so anyone can rebuild the
DLL and get the exact same file:

    py -3 -m pip install ziglang==0.16.0
    py -3 build.py --test

The DLL lands in out/lid-classpassives.dll and its SHA-256 is printed; compare it with
the one on the release page.
"""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ZIG_VERSION = "0.16.0"
REPO = Path(__file__).resolve().parent
DLL = REPO / "dll"
TESTS = REPO / "tests"
OUT = REPO / "out"

# The release package (see package() below).
PACKAGE = "LiD-Fighter-Class-Passives"
PACKAGE_NAME = "Fighter Class Passives"
PACKAGE_MODS = ["fighter-passives"]
PACKAGE_REPLACES = ["lid-fighter.dll"]   # older plugin names the manager removes for this one


def zig(*args, cwd):
    subprocess.run([sys.executable, "-m", "ziglang", *map(str, args)], cwd=cwd, check=True)


def check_zig():
    try:
        found = subprocess.run([sys.executable, "-m", "ziglang", "version"],
                               capture_output=True, text=True, check=True).stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        sys.exit(f"Zig is missing. Install it with: py -3 -m pip install ziglang=={ZIG_VERSION}")
    if found != ZIG_VERSION:
        sys.exit(f"Zig {found} is installed, but the releases are built with {ZIG_VERSION}; "
                 f"a different version gives a different file. "
                 f"Run: py -3 -m pip install ziglang=={ZIG_VERSION}")


def version():
    text = (DLL / "version.h").read_text()
    return re.search(r'#define LID_DLL_VERSION\s+"([^"]+)"', text).group(1)


def build_dll():
    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir()
    zig("cc", "-shared", "-target", "x86_64-windows-gnu", "-O2", "-s", "-o", "lid-classpassives.dll",
        DLL / "lid_classpassives.c", DLL / "version.rc", cwd=OUT)
    (OUT / "lid-classpassives.lib").unlink(missing_ok=True)   # import library, not needed
    return OUT / "lid-classpassives.dll"


def run_test(name, sources, dll):
    """Build a stand-in game exe in its own folder next to a copy of the DLL, and run it."""
    folder = OUT / "test" / name
    folder.mkdir(parents=True)
    shutil.copy(dll, folder)
    zig("cc", "-target", "x86_64-windows-gnu", "-O1", "-o", "BrgGame-Steam.exe", *sources, cwd=folder)
    print(f"\n--- test: {name}", flush=True)
    return subprocess.run([folder / "BrgGame-Steam.exe"], cwd=folder).returncode == 0


# -- the release package -------------------------------------------------------
#
# One zip with everything the LET IT DIE DB Mod Manager needs to install this
# from its "Get mods" window: lid-package.json saying what is in it, the
# manager mod(s) under mods/, and the plugin under lid-plugins/. Unpacked by
# hand it reads the same way: mods/ go in the manager's mods folder,
# lid-plugins/ in the game's Binaries\Win64. Same source, same zip: entries are
# sorted and dated 2026-01-01, so it rebuilds byte for byte.

PACKAGE_TIME = (2026, 1, 1, 0, 0, 0)


def _add(archive, name, data):
    info = zipfile.ZipInfo(name, PACKAGE_TIME)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o644 << 16
    archive.writestr(info, data)


def package(plugins=()):
    """Write out/<PACKAGE>.zip and check it. ``plugins`` are built files in out/."""
    manifest = {
        "format": 1,
        "name": PACKAGE_NAME,
        "version": version(),
        "mods": list(PACKAGE_MODS),
        "plugins": [Path(p).name for p in plugins],
        "replaces": list(PACKAGE_REPLACES),
        "loader": "",
    }
    entries = {"lid-package.json": (json.dumps(manifest, indent=2) + "\n").encode()}
    for mod in PACKAGE_MODS:
        folder = REPO / "mods" / mod
        if not (folder / "mod.json").is_file():
            sys.exit(f"mods/{mod} has no mod.json - nothing packaged")
        for path in sorted(folder.rglob("*")):
            if path.is_file() and "__pycache__" not in path.parts:
                entries[f"mods/{mod}/{path.relative_to(folder).as_posix()}"] = path.read_bytes()
    for plugin in plugins:
        entries[f"lid-plugins/{Path(plugin).name}"] = Path(plugin).read_bytes()

    out = OUT / f"{PACKAGE}.zip"
    with zipfile.ZipFile(out, "w") as archive:
        for name in sorted(entries):
            _add(archive, name, entries[name])
    with zipfile.ZipFile(out) as archive:     # read back what was written
        got = {name: archive.read(name) for name in archive.namelist()}
    if got != entries:
        sys.exit(f"{out.name} does not read back as written")
    print(f"{out.name}: {len(entries)} files, SHA-256 {hashlib.sha256(out.read_bytes()).hexdigest()}")
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--test", action="store_true", help="also run the tests")
    parser.add_argument("--tag", help="fail unless this release tag (v1.2.3) matches dll/version.h")
    parser.add_argument("--package", action="store_true",
                        help="also write out/LiD-Fighter-Class-Passives.zip, the release package")
    args = parser.parse_args()

    if args.tag and args.tag != "v" + version():
        sys.exit(f"Tag {args.tag} does not match dll/version.h ({version()}). Update one of them.")
    check_zig()
    dll = build_dll()

    if args.test:
        harness = OUT / "harness.c"
        subprocess.run([sys.executable, TESTS / "make_harness.py", harness], check=True)
        ok = run_test("patches", [harness], dll)
        ok = run_test("unknown-game", [TESTS / "nocode.c"], dll) and ok
        ok = run_test("old-name-beside", [TESTS / "oldname.c"], dll) and ok
        shutil.rmtree(OUT / "test")
        harness.unlink()
        if not ok:
            sys.exit("\nTESTS FAILED")

    if args.package:
        package(plugins=[dll])

    digest = hashlib.sha256(dll.read_bytes()).hexdigest()
    print(f"\nlid-classpassives.dll {version()}  SHA-256 {digest}")


if __name__ == "__main__":
    main()
