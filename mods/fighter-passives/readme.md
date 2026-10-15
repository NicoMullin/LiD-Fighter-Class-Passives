# Fighter Class Passives

Give each fighter class a skill decal that is always on, without taking one of
its decal slots, and, if you like, the full pause-map reveal on top.

Everything is chosen in the **Configuration** tab: for each of the eight
classes, a decal from the list (or None) and a "Reveal everything on the pause
map" switch. Nothing changes until you pick something and click **Save Mod
List**.

## What it needs

The mod has two parts, and the manager installs both:

- **The passives**, written into `masters.db`.
- **The passive hook in `BrgGame.upk`**, in `tfc/` as a `.PackagePatch`. When
  the game gathers a fighter's skills, it also looks up `SKL_FTYPE_<class>`
  (the decal you chose) and `SKL_FMAP_<class>` (the map reveal). The manager
  applies it to your own `BrgGame.upk` alongside other mods that change that
  file, such as Instant Drops, and puts the stock file back when every mod
  that changes it is switched off. Like any changed game package, it needs the
  game's file check switched off (Tools > Hash Patcher).

And one piece goes in the game folder:

- **`lid-classpassives.dll` in `Binaries\Win64\lid-plugins`**, a plugin for the
  [LiD Plugin Loader](https://github.com/NicoMullin/LiD-Plugin-Loader), which
  is installed once and loads every plugin in that folder. The pause map only
  asks the save whether Treasure Hunter, Spy, PATROL DUTY, Iwazaru or Coyote
  Smith is equipped. The plugin makes it accept the passive too, in memory
  only - the game's exe is not changed. Only the map reveals need it; every
  other passive works without it.

  The manager's **Get mods...** window puts it there (with the loader, if you
  do not have it yet) when it gets this mod from
  [its GitHub page](https://github.com/NicoMullin/LiD-Fighter-Class-Passives/releases);
  by hand, it is in that release's `LiD-Fighter-Class-Passives.zip`.

## Good to know

- A passive works like the equipped decal: its conditions still apply (a weapon
  type, low HP, and so on), and the fighter's stat screen does not show decal
  effects - it never does, even for an equipped one.
- Decals that only work "when attached to Defense Fighters, Hunters, or Haters"
  do nothing for the fighter you play.
- PS4-only decals (marked in their description) work on Steam as a passive: the
  copy is made a Steam row.
- Haters of a class carry that class's passive too: the game gives them their
  class and builds their skills the same way, just as an edited decal, weapon
  or armor changes theirs as well.
- The class description in the fighter shop gains "Passive: ..." (English text
  only).
- Switching the mod off, or setting a class back to None, removes its rows.

## Rebuilding

`decals.json`, `passives.sql` and `mod.json` are written by
`tools/build_fighter_passives.py` from a vanilla `masters.db`. Run it again
after a game update that adds decals.

The hook, `tfc/Game/BrgGame/CookedPCConsole/BrgGame.upk.PackagePatch`, is
written by `tools/build_passive_hook.py` from the game's stock `BrgGame.upk`.
It holds only the new `GetSkillArray` function and checks the names and
objects it relies on before anything is applied, so after a game update that
moves them the manager refuses it rather than forcing it. Run the tool again
on the new `BrgGame.upk` then.

## Switched with this mod

From LiD DB Editor 0.12.8, saving with this mod ticked switches
`lid-classpassives.dll` on, and saving with it unticked switches it off:
renamed to `.dll.off`, which the Plugin Loader skips. Nothing is deleted, and
ticking the mod again brings it back. Close the game first. The plugin is only
needed for the map reveals; without it the manager only mentions it.
