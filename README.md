# LiD Fighter Mod

Mods for the fighter classes in Let It Die (Steam), for use with
[LiD DB Editor](https://github.com/NicoMullin/LiD-DB-Editor).

- **Fighter Class Passives** (`mods/fighter-passives`): give each fighter class
  a skill decal that is always on, without taking a decal slot, and optionally
  the full pause-map reveal. Chosen per class in the mod manager's
  Configuration tab. It brings its own hook for `BrgGame.upk`, which the mod
  manager applies.
- **dinput8.dll** (`dll/`): makes the pause map accept those reveals. Only the
  map reveals need it; every other passive works without it.

## Why the DLL is built by GitHub

A DLL can do anything, so you should not have to take one on trust. Every
release on this page is built by GitHub Actions from the source in this
repository, on GitHub's own machines, and the tests run before anything is
published. The release page links to that build, and you can check the DLL in
two ways:

1. **Signed build record.** Each release DLL has an attestation: a signed
   record, kept by GitHub, of the exact workflow run and commit that built it.
   With the [GitHub CLI](https://cli.github.com/):

   ```
   gh attestation verify dinput8.dll --repo NicoMullin/LiD-Fighter-Mod
   ```

2. **Rebuild it yourself.** The build is reproducible: the same source and the
   same compiler give a byte-identical file. See
   [Building it yourself](#building-it-yourself) and compare the SHA-256 with
   the one on the release page.

## What the DLL does

The game loads `dinput8.dll` from its own folder before the one in Windows.
This one passes every DirectInput call straight through to the real
`dinput8.dll` in the Windows system folder, and when the game starts it makes
a few small changes to the game's code **in memory only**. The exe on disk is
never touched.

The pause map normally asks only the save file whether Treasure Hunter, Spy,
PATROL DUTY, Iwazaru or Coyote Smith is equipped. The DLL makes it also accept
the fighter's passive:

| Decal           | Pause map shows          |
|-----------------|--------------------------|
| Treasure Hunter | everything hidden        |
| Spy             | everything hidden        |
| PATROL DUTY     | enemies                  |
| Iwazaru         | escalators and elevators |
| Coyote Smith    | items                    |

Safety rules, all in [dll/lid_dinput8.c](dll/lid_dinput8.c):

- It only does anything inside `BrgGame-Steam.exe`. Loaded by any other
  program, it only passes DirectInput through.
- Every piece of game code is found by its exact bytes and must be found
  exactly once. If a game update moves the code, the DLL changes nothing and
  says so in its log.
- No network, no registry. The only file it writes is `lid_dll.log` next to
  itself.
- Delete `dinput8.dll` and the game is exactly stock again.

## Installing

1. Download `dinput8.dll` and `fighter-passives.zip` from the
   [latest release](https://github.com/NicoMullin/LiD-Fighter-Mod/releases/latest).
2. Put `dinput8.dll` in the game's `Binaries\Win64` folder, next to
   `BrgGame-Steam.exe`. If a `dinput8.dll` is already there, it belongs to
   another mod: don't overwrite it.
3. In LiD DB Editor, switch the game's file check off for `BrgGame.upk`
   (Tools > Hash Patcher) if it is not already - the mod changes that package.
4. Drag `fighter-passives.zip` onto LiD DB Editor, pick the passives in the
   Configuration tab and click **Save Mod List**. The manager writes the
   passives into `masters.db` and adds the hook to your `BrgGame.upk`.

See the mod's [readme](mods/fighter-passives/readme.md) for the details.

## Building it yourself

You need Windows and Python 3. The compiler is Zig, installed with pip; no
Visual Studio needed.

```
py -3 -m pip install ziglang==0.16.0
py -3 build.py --test
```

The DLL lands in `out\dinput8.dll` and the SHA-256 is printed at the end. Use
Zig 0.16.0 exactly: another version builds a working DLL, but not a
byte-identical one.

`--test` builds two stand-ins for the game exe and runs them against the DLL:

- `tests/harness.in.c` holds the game's code shapes (the skill loop, the
  status reset and the pause map's checks) and checks every change the DLL
  makes, including that a fighter without the passives gets nothing.
- `tests/nocode.c` holds none of them, as after a game update, and checks the
  DLL changes nothing and still passes DirectInput through.

## Making a release

1. Set the new version in [dll/version.h](dll/version.h).
2. Push a tag with the same version, for example `v1.0.1`.

GitHub builds, tests and publishes the release. The build stops if the tag and
`version.h` don't match.

## Rebuilding the mod files

`decals.json`, `passives.sql` and `mod.json` in `mods/fighter-passives` are
written from a vanilla `masters.db` by
[tools/build_fighter_passives.py](tools/build_fighter_passives.py). Run it
again after a game update that adds decals.

The hook, `mods/fighter-passives/tfc/.../BrgGame.upk.PackagePatch`, is written
from the game's stock `BrgGame.upk` by
[tools/build_passive_hook.py](tools/build_passive_hook.py), which uses the
package reader from a [LiD DB Editor](https://github.com/NicoMullin/LiD-DB-Editor)
checkout:

```
py -3 tools/build_passive_hook.py --manager PATH\TO\LiD-DB-Editor --upk PATH\TO\BrgGame.upk
```

It holds only the new `GetSkillArray` function, never the game's package, and
checks itself by applying the patch before it writes it. Run it again after a
game update that changes `BrgGame.upk`.

## Credit

The `.PackagePatch` format is FCH823's, from
[TFC Installer](https://www.nexusmods.com/site/mods/587); LiD DB Editor applies
it with their permission.

## License

MIT - see [LICENSE](LICENSE). Made by KSFA.
