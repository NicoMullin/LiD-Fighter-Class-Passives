"""Write the passive hook for BrgGame.upk as a .PackagePatch.

The hook goes at the end of BrgCommonPawn_CustomChara.GetSkillArray, the
function that gathers a fighter's skills, just before its `return true`:

    if (Database.GetSkill("SKL_FTYPE_" $ mFighterType, Skill) == true)
        SkillArray.AddItem(Skill);
    if (Database.GetSkill("SKL_FMAP_" $ mFighterType, Skill) == true)
        SkillArray.AddItem(Skill);

The first is the decal a player chose for the class, the second the pause-map
reveal. Both rows are written into masters.db by passives.sql; a class with no
row is skipped, so with the mod off the hook does nothing.

A .PackagePatch holds only the new function, not the game's package. LiD DB
Editor applies it to the player's own BrgGame.upk (the format is FCH823's, from
TFC Installer). The patch names every object and name the new function relies
on, so it is refused, not forced, on a package where any of them moved.

Reads the game's stock BrgGame.upk and never changes it. Needs a LiD DB Editor
checkout for its package reader:

    py -3 tools/build_passive_hook.py --manager PATH/TO/LiD-DB-Editor --upk PATH/TO/BrgGame.upk

The stock BrgGame.upk is the one Steam installs, or the copy LiD DB Editor
keeps in backups/game_files once a mod has changed it.
"""
from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
OUT = REPO / "mods" / "fighter-passives" / "tfc" / "Game" / "BrgGame" / "CookedPCConsole" / \
    "BrgGame.upk.PackagePatch"
PREFIXES = [b"SKL_FTYPE_", b"SKL_FMAP_"]
HEADER = 48                     # GetSkillArray's data before its script
STOCK_SIZES = (34, 855, 555)    # statements, script bytes in memory, on disk - game 1.89
REFERENCE_VERSION = 12          # the reference layout TFC Installer writes

parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
parser.add_argument("--manager", required=True, type=Path, help="a LiD DB Editor checkout")
parser.add_argument("--upk", required=True, type=Path, help="the game's stock BrgGame.upk")
parser.add_argument("--out", type=Path, default=OUT)
args = parser.parse_args()

sys.path.insert(0, str(args.manager.resolve()))
sys.path.insert(0, str(HERE))
from lid_db_manager.upk import apply as A                   # noqa: E402
from lid_db_manager.upk import package as P                 # noqa: E402
from lid_db_manager.upk import packagepatch as PP           # noqa: E402
from ue3script import Walker                                # noqa: E402

raw = args.upk.read_bytes()
print(f"{args.upk.name}: SHA-1 {hashlib.sha1(raw).hexdigest()}")
summary, flat = P.decompressed(raw)
pkg = P.Package(summary, flat, P.read_names(flat, summary), P.read_imports(flat, summary),
                P.read_exports(flat, summary), source=args.upk)


def export(path: str) -> int:
    """1-based export reference by full path; exactly one must match."""
    hits = [i + 1 for i in range(len(pkg.exports)) if pkg.full_path(i + 1) == path]
    if len(hits) != 1:
        sys.exit(f"{path}: {len(hits)} matches - is this BrgGame.upk?")
    return hits[0]


FUNC = export("BrgCommonPawn_CustomChara.GetSkillArray")
DATABASE = export("BrgCommonPawn_CustomChara.GetSkillArray.Database")
SKILL = export("BrgCommonPawn_CustomChara.GetSkillArray.Skill")
SKILLARRAY = export("BrgCommonPawn_CustomChara.GetSkillArray.SkillArray")
FTYPE = export("BrgCommonPawn_CustomCharaNative.mFighterType")
GETSKILL_RET = export("BrgDatabase.GetSkill.ReturnValue")
GETSKILL_NAME = next(i for i, n in enumerate(pkg.names) if n.text == "GetSkill")

entry = pkg.exports[FUNC - 1]
data = pkg.export_data(FUNC - 1)
mem_size, disk_size = struct.unpack_from("<ii", data, HEADER - 8)
code, tail = data[HEADER:HEADER + disk_size], data[HEADER + disk_size:]

names = lambda i: pkg.names[i].text  # noqa: E731
objs = lambda i: pkg.full_path(i)     # noqa: E731
lines = Walker(code, names, objs).run()
if (len(lines), mem_size, disk_size) != STOCK_SIZES:
    sys.exit(f"GetSkillArray is not the one this was written for: {len(lines)} statements, "
             f"{mem_size}/{disk_size} bytes. Is this the stock BrgGame.upk of game 1.89?")
# the loop exit: the `return(true)` statement the copy-skill loop jumps to
ret_disk, ret_mem, ret_text = lines[-3]
if ret_text != "return(true)" or ret_mem != 0x34A:
    sys.exit(f"unexpected statement at the loop exit: {ret_text} @ {ret_mem:#x}")

o = lambda ref: struct.pack("<i", ref)  # noqa: E731


def mem_len(chunk: bytes) -> int:
    w = Walker(chunk + b"\x53", names, objs)
    w.run()
    return w.mem - 1


def statement(prefix: bytes, at_mem: int) -> bytes:
    """if (Database.GetSkill(prefix $ mFighterType, Skill) == true) SkillArray.AddItem(Skill);"""
    call = (b"\x1b" + struct.pack("<ii", GETSKILL_NAME, 0)                    # GetSkill(
            + b"\x70" + b"\x1f" + prefix + b"\0" + b"\x01" + o(FTYPE) + b"\x16"  # prefix $ mFighterType
            + b"\x00" + o(SKILL)                                               # , Skill
            + b"\x16")                                                         # )
    call_mem = 1 + 8 + (1 + 1 + len(prefix) + 1 + 1 + 8 + 1) + (1 + 8) + 1
    context = b"\x19" + b"\x00" + o(DATABASE) + struct.pack("<H", call_mem) + o(GETSKILL_RET) + b"\x00" + call
    cond = b"\xf2" + context + b"\x27" + b"\x16"                              # (...) == true
    additem = b"\x55" + b"\x48" + o(SKILLARRAY) + struct.pack("<H", 10) + b"\x00" + o(SKILL) + b"\x16"
    after = at_mem + 3 + mem_len(cond) + mem_len(additem)                      # the next statement
    return b"\x07" + struct.pack("<H", after) + cond + additem


insert = b""
at = ret_mem
starts = []
for prefix in PREFIXES:
    starts.append(at)
    piece = statement(prefix, at)
    insert += piece
    at += mem_len(piece)
target = at                       # where the last jump lands: `return(true)`, moved along

new_code = code[:ret_disk] + insert + code[ret_disk:]
new_mem, new_disk = mem_size + mem_len(insert), disk_size + len(insert)

# the new script reads back cleanly and every jump lands on a statement
used_names, used_objects = set(), set()
check = Walker(new_code, lambda i: used_names.add(i) or names(i),
               lambda i: used_objects.add(i) or objs(i)).run()
w = Walker(new_code, names, objs)
w.run()
if check[-1][2] != "END" or (w.mem, w.pos) != (new_mem, new_disk):
    sys.exit("the new script does not read back cleanly")
if [t for d, m, t in check if m == target] != ["return(true)"]:
    sys.exit(f"the last jump, to {target:#x}, does not land on return(true)")
for start in starts[1:]:
    if not [t for d, m, t in check if m == start]:
        sys.exit(f"a jump lands at {start:#x}, which is not the start of a statement")

new_data = data[:HEADER - 8] + struct.pack("<ii", new_mem, new_disk) + new_code + tail


# ---------------------------------------------------------------- the patch

def fstring(text: str) -> bytes:
    encoded = text.encode("latin-1") + b"\0"
    return struct.pack("<i", len(encoded)) + encoded


def class_of(ref: int) -> tuple[str, str]:
    """(class name, class package) the way the patch format records them."""
    if ref < 0:
        imp = pkg.imports[-ref - 1]
        return pkg.name(imp.class_name, imp.class_name_number), \
            pkg.name(imp.class_package, imp.class_package_number)
    class_index = pkg.exports[ref - 1].class_index
    return (pkg.object_name(class_index) if class_index else "Class"), ""


def path_of(ref: int) -> str:
    return pkg.full_path(ref).replace(".", "\\")


object_refs = sorted({FUNC} | {r for r in used_objects if r != 0}, key=lambda r: (r < 0, abs(r)))
name_refs = sorted(used_names)

out = bytearray()
out += struct.pack("<i", 2)                                     # patch format version
for table in (pkg.names, pkg.imports, pkg.exports):
    out += struct.pack("<ii", len(table), 0)                    # original length, no changes
out += struct.pack("<i", 1)                                     # one object
out += struct.pack("<ii", FUNC - 1, 0)                          # GetSkillArray, no offset records
out += struct.pack("<i", len(new_data)) + new_data
out += struct.pack("<i", REFERENCE_VERSION)
out += struct.pack("<i", len(name_refs))
for i in name_refs:
    out += struct.pack("<i", i) + fstring(pkg.names[i].text)
out += struct.pack("<i", len(object_refs))
for ref in object_refs:
    class_name, class_package = class_of(ref)
    out += struct.pack("<i", ref) + fstring(path_of(ref))
    if ref < 0:
        out += fstring(path_of(ref))
    out += fstring(class_name)
    if ref < 0:
        out += fstring(class_package)
patch_bytes = bytes(out)

# ------------------------------------------------- check it before writing it

old_at, old_size = entry.serial_offset, entry.serial_size
patch = PP.read(patch_bytes)
if A.problems(pkg, patch):
    sys.exit("the patch does not fit the package it was made from: " + "; ".join(A.problems(pkg, patch)))
applied, _ = A.apply(pkg, patch)
# what the hook did to the package before it was a patch: the old function
# blanked, the new one on the end, its export entry pointed there
expected = bytearray(flat)
expected[old_at:old_at + old_size] = bytes(old_size)
moved_to = len(expected)
expected += new_data
A._place_entry(expected, P.read_exports(flat, summary)[FUNC - 1], moved_to, len(new_data))
if applied != bytes(expected):
    sys.exit("applying the patch does not give the hooked package")

args.out.parent.mkdir(parents=True, exist_ok=True)
args.out.write_bytes(patch_bytes)
print(f"GetSkillArray: {mem_size}/{disk_size} -> {new_mem}/{new_disk} script bytes (memory/disk), "
      f"{old_size} -> {len(new_data)} bytes in all")
print(f"checks: {len(name_refs)} name(s), {len(object_refs)} object(s)")
for ref in object_refs:
    print(f"  {ref:>7}  {path_of(ref)}  ({class_of(ref)[0]})")
print(f"wrote {args.out} ({len(patch_bytes):,} bytes), SHA-256 {hashlib.sha256(patch_bytes).hexdigest()}")
