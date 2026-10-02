"""Write the Fighter Class Passives mod from a vanilla masters.db.

A maintenance tool. It produces three files in ``mods/fighter-passives``:

* ``decals.json`` - every skill decal the game has, once each, as the list a
  player picks from. Names, groups and effect text come from the database's own
  English text, with the decal's numbers filled in.
* ``passives.sql`` - one block per fighter class: copy the chosen decal into a
  hidden ``SKL_FTYPE_<class>`` row, and Treasure Hunter into ``SKL_FMAP_<class>``
  when the map reveal is on. The passive hook in BrgGame.upk looks both up.
* ``mod.json`` - the mod, with a decal choice and a map switch per class.

Run it again after a game update adds decals. Nothing in the database is
changed; it is opened read-only.

Usage::

    py -3 tools/build_fighter_passives.py --db "C:/.../5.0.4.2/masters.db"
"""

from __future__ import annotations

import argparse
import json
import re
import sqlite3
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
OUT = REPO / "mods" / "fighter-passives"

# In the order the fighter shop opens them. OLD is left out on purpose: it is
# the type the game gives spawned enemies, which would get the passive too.
CLASSES = [
    ("BAL", "All-rounder", "TXT_BAL"),
    ("BRE", "Striker", "TXT_BRE"),
    ("COL", "Collector", "TXT_COL"),
    ("TEC", "Attacker", "TXT_TEC"),
    ("DEF", "Defender", "TXT_DEF"),
    ("SHT", "Shooter", "TXT_SHT"),
    ("LUK", "Lucky Star", "TXT_LUC"),  # the shop text is filed under LUC
    ("SKI", "Skill Master", "TXT_SKI"),
]

MAP_SOURCE = "SKL_SEARCHUP_ITEM_P"  # Treasure Hunter: the reveal and nothing else

COLUMNS = (
    "id, no, display_label, name, type, desc, getroute, exp, "
    "val0, val1, val2, val3, val4, val5, premium, is_nosale, is_display, is_display_list, "
    "start, end, rarity, battle_rate, buy_money, buy_spirit, buy_recycle_point, "
    "sell_recycle_point, sell_money, category, unlock, no_steam, platform"
)
# A hidden copy: never sold, never listed, free, and a Steam row whatever the
# source was - the Steam game skips PS4-only rows (platform 1) when it loads.
COPY = (
    "(SELECT MAX(no) + 1 FROM master_skill), display_label, name, type, desc, getroute, exp, "
    "val0, val1, val2, val3, val4, val5, premium, 1, 0, 0, "
    "start, end, rarity, battle_rate, 0, 0, 0, 0, 0, category, unlock, 0, 0"
)


def text(con: sqlite3.Connection, key: str) -> str:
    """The English text for a "SECTION.ID" key."""
    if not key or "." not in key:
        return key or ""
    section, text_id = key.split(".", 1)
    row = con.execute(
        "SELECT txt FROM master_text WHERE sct = ? AND id = ? AND lang = 'int'", (section, text_id)
    ).fetchone()
    return row[0] if row else ""


def decal_list(con: sqlite3.Connection) -> list[dict]:
    categories = {
        cat_id: text(con, name) for cat_id, name in con.execute("SELECT id, name FROM master_skill_category")
    }
    seen: dict[tuple[str, str], dict] = {}
    rows = con.execute(
        "SELECT id, name, desc, val0, val1, val2, val3, val4, val5, category, platform "
        "FROM master_skill ORDER BY platform, length(id), id"
    ).fetchall()
    for skill_id, name_key, desc_key, *rest in rows:
        values, category, platform = rest[:6], rest[6], rest[7]
        name = text(con, name_key).strip() or skill_id
        desc = text(con, desc_key).replace("//", " ").replace("\n", " ")
        desc = re.sub(r"#(\d)", lambda m: str(values[int(m.group(1))]), desc)
        desc = re.sub(r"\s+", " ", desc).strip()
        key = (name.lower(), desc)
        if key in seen:
            continue  # the same decal twice (a plain and a _P copy): list it once
        group = categories.get(category, "")
        if not group or group.endswith("!"):
            group = "Other"
        if platform:
            desc += " (A PS4-only decal; it works as a passive on Steam too.)"
        seen[key] = {"value": skill_id, "label": name, "group": group, "help": desc}

    options = list(seen.values())
    # Two different decals with one name: tell them apart by their id.
    names: dict[str, int] = {}
    for option in options:
        names[option["label"].lower()] = names.get(option["label"].lower(), 0) + 1
    for option in options:
        if names[option["label"].lower()] > 1:
            option["label"] = f"{option['label']} ({option['value']})"
    options.sort(key=lambda o: (o["group"], o["label"].lower()))
    none = {"value": "", "label": "None", "help": "No passive decal for this class."}
    return [none] + options


def sql_block(code: str, name: str, text_id: str) -> str:
    low = code.lower()
    skill, reveal = f"{{{{{low}_skill}}}}", f"{{{{{low}_map}}}}"
    return f"""-- {name} ({code}) {'-' * (60 - len(name) - len(code))}

INSERT OR REPLACE INTO master_skill ({COLUMNS})
SELECT 'SKL_FTYPE_{code}', {COPY}
FROM master_skill WHERE id = '{skill}';

INSERT OR REPLACE INTO master_skill ({COLUMNS})
SELECT 'SKL_FMAP_{code}', {COPY}
FROM master_skill WHERE id = '{MAP_SOURCE}' AND {reveal} = 1;

-- Say so in the fighter shop (English text only).
UPDATE master_text
SET txt = txt || ' Passive: '
    || CASE WHEN '{skill}' <> '' THEN COALESCE((
           SELECT t.txt FROM master_skill s JOIN master_text t
             ON t.sct = 'SKILL_NAME' AND 'SKILL_NAME.' || t.id = s.name AND t.lang = 'int'
           WHERE s.id = '{skill}'), '{skill}') ELSE '' END
    || CASE WHEN '{skill}' <> '' AND {reveal} = 1 THEN ' + ' ELSE '' END
    || CASE WHEN {reveal} = 1 THEN 'map reveal' ELSE '' END
    || '.'
WHERE sct = 'FTYPE_DSC' AND id = '{text_id}' AND lang = 'int'
  AND ('{skill}' <> '' OR {reveal} = 1);
"""


def passives_sql() -> str:
    head = """-- Fighter Class Passives - written by tools/build_fighter_passives.py.
--
-- For each class: a hidden copy of the chosen decal as SKL_FTYPE_<class>, and
-- Treasure Hunter as SKL_FMAP_<class> when the map reveal is on. The passive
-- hook in BrgGame.upk adds both to the fighter's skills; dinput8.dll makes the
-- pause map honour the map reveals. Every {{...}} is filled in by the mod
-- manager from the Configuration tab: a decal id (letters, digits and
-- underscores only) or 1/0.

"""
    blocks = "\n".join(sql_block(code, name, text_id) for code, name, text_id in CLASSES)
    tail = """
-- The Steam game wants a Steam number on every row it loads.
UPDATE master_skill SET no_steam = no
WHERE id GLOB 'SKL_FTYPE_*' OR id GLOB 'SKL_FMAP_*';
"""
    return head + blocks + tail


def mod_json(game_version: str) -> dict:
    settings = []
    for code, name, _text_id in CLASSES:
        low = code.lower()
        settings.append({
            "id": f"{low}_skill",
            "label": "Passive decal",
            "type": "choice",
            "group": name,
            "default": "",
            "options_from": "decals.json",
        })
        settings.append({
            "id": f"{low}_map",
            "label": "Reveal everything on the pause map",
            "type": "toggle",
            "group": name,
            "default": False,
            "help": "The Treasure Hunter reveal on its own, on top of the decal above.",
        })
    return {
        "id": "fighter-passives",
        "name": "Fighter Class Passives",
        "description": (
            "Give each fighter class a skill decal that is always on, without taking a "
            "decal slot, and optionally the full pause-map reveal. Adds its own passive "
            "hook to BrgGame.upk; the map reveals also need dinput8.dll - see the readme."
        ),
        "version": "0.2.0",
        "author": "KSFA",
        "category": "Game content",
        "tags": ["fighters", "decals", "passives", "map"],
        "game_version": [game_version] if game_version else [],
        "apply": "diff",
        "requires": [],
        "conflicts_with": [],
        "settings": settings,
        "patches": [{
            "type": "raw_sql_file",
            "path": "passives.sql",
            "description": "Class passives and map reveals",
        }, {
            # written by tools/build_passive_hook.py
            "type": "tfc_installer",
            "source": "tfc",
            "description": "The passive hook in BrgGame.upk",
        }],
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--db", required=True, help="a vanilla masters.db")
    args = parser.parse_args()
    con = sqlite3.connect(f"file:{Path(args.db).as_posix()}?mode=ro", uri=True)
    version_row = con.execute(
        "SELECT value FROM master_const_str WHERE id = 'TITLE_VERSION'"
    ).fetchone()
    options = decal_list(con)
    con.close()

    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "decals.json").write_text(json.dumps(options, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    (OUT / "passives.sql").write_text(passives_sql(), encoding="utf-8")
    (OUT / "mod.json").write_text(
        json.dumps(mod_json(version_row[0] if version_row else ""), ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"{len(options) - 1} decals, {len(CLASSES)} classes -> {OUT}")


if __name__ == "__main__":
    main()
