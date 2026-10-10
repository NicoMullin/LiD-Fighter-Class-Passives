# LiD Fighter Class Passives

A passive skill for each fighter class in Let It Die (Steam), for use with
[LiD DB Editor](https://github.com/NicoMullin/LiD-DB-Editor). This repository
used to be called LiD Fighter Mod.

- **Fighter Class Passives** (`mods/fighter-passives`): give each fighter class
  a skill decal that is always on, without taking a decal slot, and optionally
  the full pause-map reveal. Chosen per class in the mod manager's
  Configuration tab. It brings its own hook for `BrgGame.upk`, which the mod
  manager applies.
- **lid-classpassives.dll** (`dll/`): a plugin for the
  [LiD Plugin Loader](https://github.com/NicoMullin/LiD-Plugin-Loader) that
  makes the pause map accept those reveals. Only the map reveals need it; every
  other passive works without it.

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
   gh attestation verify lid-classpassives.dll --repo NicoMullin/LiD-Fighter-Class-Passives
   ```

2. **Rebuild it yourself.** The build is reproducible: the same source and the
   same compiler give a byte-identical file. See
   [Building it yourself](#building-it-yourself) and compare the SHA-256 with
   the one on the release page.

## What the DLL does

The LiD Plugin Loader (a small `dinput8.dll` next to the game) loads
`lid-classpassives.dll` from the `lid-plugins` folder when the game starts. The
plugin then makes a few small changes to the game's code **in memory only**.
The exe on disk is never touched. Other plugins can sit in the same folder:
this one only touches the pause map and the skill code.

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

Safety rules, all in [dll/lid_classpassives.c](dll/lid_classpassives.c):

- It only does anything inside `BrgGame-Steam.exe`. Loaded by any other
  program, it changes nothing.
- Every piece of game code is found by its exact bytes and must be found
  exactly once. If a game update moves the code, the DLL changes nothing and
  says so in its log.
- No network, no registry. The only file it writes is `lid-classpassives.log` next
  to itself.
- Delete `lid-classpassives.dll` and the game is exactly stock again.

## Installing

**The easy way.** In LiD DB Editor (the LET IT DIE DB Mod Manager) 0.12.6 or
newer, close the game, click **Get mods...** and then **Get** next to **Fighter
Class Passives**. It downloads this repository's newest release, checks it
against the SHA-256 GitHub lists for it, and once you say yes puts the
fighter-passives mod in the manager and `lid-classpassives.dll` in the game's
`Binaries\Win64\lid-plugins` - with the [LiD Plugin
Loader](https://github.com/NicoMullin/LiD-Plugin-Loader) too, if you do not
have it yet. Then switch the game's file check off for `BrgGame.upk` (Tools >
Hash Patcher) if it is not already, tick **fighter-passives** and click **Save
Mod List**.

**By hand.**

1. Install the [LiD Plugin Loader](https://github.com/NicoMullin/LiD-Plugin-Loader)
   once, if you have not already. It puts a `dinput8.dll` and a `lid-plugins`
   folder in the game's `Binaries\Win64` folder.
2. Download `LiD-Fighter-Class-Passives.zip` from the
   [latest release](https://github.com/NicoMullin/LiD-Fighter-Class-Passives/releases/latest).
   Put `lid-plugins\lid-classpassives.dll` from it in the game's `Binaries\Win64\lid-plugins`.
3. In the manager, switch the game's file check off for `BrgGame.upk` (Tools >
   Hash Patcher) if it is not already, drop the `mods\fighter-passives` folder
   from the zip onto it, tick it and click **Save Mod List**.

The release also has `lid-classpassives.dll` on its own, for anyone who only wants the plugin.

Only the pause-map reveals need the plugin; every other passive works with the
mod alone. It works alongside other plugins, such as LiD Death Metals and LiD
Timers: they change different parts of the game.

## Updating from older versions

- **`lid-fighter.dll`** was this plugin's name in earlier test builds. Delete it from
  `lid-plugins` when you put `lid-classpassives.dll` there (Get mods does that
  for you). While both are there, `lid-classpassives.dll` stands aside and says
  so in its log, so the two never make the same changes twice.
- **1.0.0 and older** were a `dinput8.dll` of their own. Install the Plugin
  Loader, which replaces that `dinput8.dll`, and the old `lid_dll.log` can be
  deleted.

## Building it yourself

You need Windows and Python 3. The compiler is Zig, installed with pip; no
Visual Studio needed.

```
py -3 -m pip install ziglang==0.16.0
py -3 build.py --test
```

The DLL lands in `out\lid-classpassives.dll` and the SHA-256 is printed at the end. Use
Zig 0.16.0 exactly: another version builds a working DLL, but not a
byte-identical one.

`--test` builds three stand-ins for the game exe and runs them against the DLL:

- `tests/harness.in.c` holds the game's code shapes (the skill loop, the
  status reset and the pause map's checks) and checks every change the DLL
  makes, including that a fighter without the passives gets nothing.
- `tests/nocode.c` holds none of them, as after a game update, and checks the
  DLL changes nothing and says so in its log.
- `tests/oldname.c` puts a `lid-fighter.dll` (the old name) beside it and
  checks the DLL stands aside before it looks at any game code.

Passing DirectInput through and loading the plugin are the loader's job, and
its own tests cover them.

## Making a release

1. Set the new version in [dll/version.h](dll/version.h).
2. Push a tag with the same version, for example `v2.0.1`.

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
