"""Build dinput8.dll, and with --test run the tests against it.

GitHub builds every release with this same script, so anyone can rebuild the
DLL and get the exact same file:

    py -3 -m pip install ziglang==0.16.0
    py -3 build.py --test

The DLL lands in out/dinput8.dll and its SHA-256 is printed; compare it with
the one on the release page.
"""
import argparse
import hashlib
import re
import shutil
import subprocess
import sys
from pathlib import Path

ZIG_VERSION = "0.16.0"
REPO = Path(__file__).resolve().parent
DLL = REPO / "dll"
TESTS = REPO / "tests"
OUT = REPO / "out"


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
    zig("cc", "-shared", "-target", "x86_64-windows-gnu", "-O2", "-s", "-o", "dinput8.dll",
        DLL / "lid_dinput8.c", DLL / "dinput8.def", DLL / "version.rc", cwd=OUT)
    (OUT / "lid_dinput8.lib").unlink(missing_ok=True)   # import library, not needed
    return OUT / "dinput8.dll"


def run_test(name, sources, dll):
    """Build a stand-in game exe in its own folder next to a copy of the DLL, and run it."""
    folder = OUT / "test" / name
    folder.mkdir(parents=True)
    shutil.copy(dll, folder)
    zig("cc", "-target", "x86_64-windows-gnu", "-O1", "-o", "BrgGame-Steam.exe", *sources, cwd=folder)
    print(f"\n--- test: {name}", flush=True)
    return subprocess.run([folder / "BrgGame-Steam.exe"], cwd=folder).returncode == 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--test", action="store_true", help="also run the tests")
    parser.add_argument("--tag", help="fail unless this release tag (v1.2.3) matches dll/version.h")
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
        shutil.rmtree(OUT / "test")
        harness.unlink()
        if not ok:
            sys.exit("\nTESTS FAILED")

    digest = hashlib.sha256(dll.read_bytes()).hexdigest()
    print(f"\ndinput8.dll {version()}  SHA-256 {digest}")


if __name__ == "__main__":
    main()
