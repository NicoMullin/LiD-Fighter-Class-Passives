-- Fighter Class Passives - written by tools/build_fighter_passives.py.
--
-- For each class: a hidden copy of the chosen decal as SKL_FTYPE_<class>, and
-- Treasure Hunter as SKL_FMAP_<class> when the map reveal is on. The passive
-- hook in BrgGame.upk adds both to the fighter's skills; dinput8.dll makes the
-- pause map honour the map reveals. Every {{...}} is filled in by the mod
-- manager from the Configuration tab: a decal id (letters, digits and
-- underscores only) or 1/0.

-- All-rounder (BAL) ----------------------------------------------

INSERT OR REPLACE INTO master_skill (id, no, display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, is_nosale, is_display, is_display_list, start, end, rarity, battle_rate, buy_money, buy_spirit, buy_recycle_point, sell_recycle_point, sell_money, category, unlock, no_steam, platform)
SELECT 'SKL_FTYPE_BAL', (SELECT MAX(no) + 1 FROM master_skill), display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, 1, 0, 0, start, end, rarity, battle_rate, 0, 0, 0, 0, 0, category, unlock, 0, 0
FROM master_skill WHERE id = '{{bal_skill}}';

INSERT OR REPLACE INTO master_skill (id, no, display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, is_nosale, is_display, is_display_list, start, end, rarity, battle_rate, buy_money, buy_spirit, buy_recycle_point, sell_recycle_point, sell_money, category, unlock, no_steam, platform)
SELECT 'SKL_FMAP_BAL', (SELECT MAX(no) + 1 FROM master_skill), display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, 1, 0, 0, start, end, rarity, battle_rate, 0, 0, 0, 0, 0, category, unlock, 0, 0
FROM master_skill WHERE id = 'SKL_SEARCHUP_ITEM_P' AND {{bal_map}} = 1;

-- Say so in the fighter shop (English text only).
UPDATE master_text
SET txt = txt || ' Passive: '
    || CASE WHEN '{{bal_skill}}' <> '' THEN COALESCE((
           SELECT t.txt FROM master_skill s JOIN master_text t
             ON t.sct = 'SKILL_NAME' AND 'SKILL_NAME.' || t.id = s.name AND t.lang = 'int'
           WHERE s.id = '{{bal_skill}}'), '{{bal_skill}}') ELSE '' END
    || CASE WHEN '{{bal_skill}}' <> '' AND {{bal_map}} = 1 THEN ' + ' ELSE '' END
    || CASE WHEN {{bal_map}} = 1 THEN 'map reveal' ELSE '' END
    || '.'
WHERE sct = 'FTYPE_DSC' AND id = 'TXT_BAL' AND lang = 'int'
  AND ('{{bal_skill}}' <> '' OR {{bal_map}} = 1);

-- Striker (BRE) --------------------------------------------------

INSERT OR REPLACE INTO master_skill (id, no, display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, is_nosale, is_display, is_display_list, start, end, rarity, battle_rate, buy_money, buy_spirit, buy_recycle_point, sell_recycle_point, sell_money, category, unlock, no_steam, platform)
SELECT 'SKL_FTYPE_BRE', (SELECT MAX(no) + 1 FROM master_skill), display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, 1, 0, 0, start, end, rarity, battle_rate, 0, 0, 0, 0, 0, category, unlock, 0, 0
FROM master_skill WHERE id = '{{bre_skill}}';

INSERT OR REPLACE INTO master_skill (id, no, display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, is_nosale, is_display, is_display_list, start, end, rarity, battle_rate, buy_money, buy_spirit, buy_recycle_point, sell_recycle_point, sell_money, category, unlock, no_steam, platform)
SELECT 'SKL_FMAP_BRE', (SELECT MAX(no) + 1 FROM master_skill), display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, 1, 0, 0, start, end, rarity, battle_rate, 0, 0, 0, 0, 0, category, unlock, 0, 0
FROM master_skill WHERE id = 'SKL_SEARCHUP_ITEM_P' AND {{bre_map}} = 1;

-- Say so in the fighter shop (English text only).
UPDATE master_text
SET txt = txt || ' Passive: '
    || CASE WHEN '{{bre_skill}}' <> '' THEN COALESCE((
           SELECT t.txt FROM master_skill s JOIN master_text t
             ON t.sct = 'SKILL_NAME' AND 'SKILL_NAME.' || t.id = s.name AND t.lang = 'int'
           WHERE s.id = '{{bre_skill}}'), '{{bre_skill}}') ELSE '' END
    || CASE WHEN '{{bre_skill}}' <> '' AND {{bre_map}} = 1 THEN ' + ' ELSE '' END
    || CASE WHEN {{bre_map}} = 1 THEN 'map reveal' ELSE '' END
    || '.'
WHERE sct = 'FTYPE_DSC' AND id = 'TXT_BRE' AND lang = 'int'
  AND ('{{bre_skill}}' <> '' OR {{bre_map}} = 1);

-- Collector (COL) ------------------------------------------------

INSERT OR REPLACE INTO master_skill (id, no, display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, is_nosale, is_display, is_display_list, start, end, rarity, battle_rate, buy_money, buy_spirit, buy_recycle_point, sell_recycle_point, sell_money, category, unlock, no_steam, platform)
SELECT 'SKL_FTYPE_COL', (SELECT MAX(no) + 1 FROM master_skill), display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, 1, 0, 0, start, end, rarity, battle_rate, 0, 0, 0, 0, 0, category, unlock, 0, 0
FROM master_skill WHERE id = '{{col_skill}}';

INSERT OR REPLACE INTO master_skill (id, no, display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, is_nosale, is_display, is_display_list, start, end, rarity, battle_rate, buy_money, buy_spirit, buy_recycle_point, sell_recycle_point, sell_money, category, unlock, no_steam, platform)
SELECT 'SKL_FMAP_COL', (SELECT MAX(no) + 1 FROM master_skill), display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, 1, 0, 0, start, end, rarity, battle_rate, 0, 0, 0, 0, 0, category, unlock, 0, 0
FROM master_skill WHERE id = 'SKL_SEARCHUP_ITEM_P' AND {{col_map}} = 1;

-- Say so in the fighter shop (English text only).
UPDATE master_text
SET txt = txt || ' Passive: '
    || CASE WHEN '{{col_skill}}' <> '' THEN COALESCE((
           SELECT t.txt FROM master_skill s JOIN master_text t
             ON t.sct = 'SKILL_NAME' AND 'SKILL_NAME.' || t.id = s.name AND t.lang = 'int'
           WHERE s.id = '{{col_skill}}'), '{{col_skill}}') ELSE '' END
    || CASE WHEN '{{col_skill}}' <> '' AND {{col_map}} = 1 THEN ' + ' ELSE '' END
    || CASE WHEN {{col_map}} = 1 THEN 'map reveal' ELSE '' END
    || '.'
WHERE sct = 'FTYPE_DSC' AND id = 'TXT_COL' AND lang = 'int'
  AND ('{{col_skill}}' <> '' OR {{col_map}} = 1);

-- Attacker (TEC) -------------------------------------------------

INSERT OR REPLACE INTO master_skill (id, no, display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, is_nosale, is_display, is_display_list, start, end, rarity, battle_rate, buy_money, buy_spirit, buy_recycle_point, sell_recycle_point, sell_money, category, unlock, no_steam, platform)
SELECT 'SKL_FTYPE_TEC', (SELECT MAX(no) + 1 FROM master_skill), display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, 1, 0, 0, start, end, rarity, battle_rate, 0, 0, 0, 0, 0, category, unlock, 0, 0
FROM master_skill WHERE id = '{{tec_skill}}';

INSERT OR REPLACE INTO master_skill (id, no, display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, is_nosale, is_display, is_display_list, start, end, rarity, battle_rate, buy_money, buy_spirit, buy_recycle_point, sell_recycle_point, sell_money, category, unlock, no_steam, platform)
SELECT 'SKL_FMAP_TEC', (SELECT MAX(no) + 1 FROM master_skill), display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, 1, 0, 0, start, end, rarity, battle_rate, 0, 0, 0, 0, 0, category, unlock, 0, 0
FROM master_skill WHERE id = 'SKL_SEARCHUP_ITEM_P' AND {{tec_map}} = 1;

-- Say so in the fighter shop (English text only).
UPDATE master_text
SET txt = txt || ' Passive: '
    || CASE WHEN '{{tec_skill}}' <> '' THEN COALESCE((
           SELECT t.txt FROM master_skill s JOIN master_text t
             ON t.sct = 'SKILL_NAME' AND 'SKILL_NAME.' || t.id = s.name AND t.lang = 'int'
           WHERE s.id = '{{tec_skill}}'), '{{tec_skill}}') ELSE '' END
    || CASE WHEN '{{tec_skill}}' <> '' AND {{tec_map}} = 1 THEN ' + ' ELSE '' END
    || CASE WHEN {{tec_map}} = 1 THEN 'map reveal' ELSE '' END
    || '.'
WHERE sct = 'FTYPE_DSC' AND id = 'TXT_TEC' AND lang = 'int'
  AND ('{{tec_skill}}' <> '' OR {{tec_map}} = 1);

-- Defender (DEF) -------------------------------------------------

INSERT OR REPLACE INTO master_skill (id, no, display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, is_nosale, is_display, is_display_list, start, end, rarity, battle_rate, buy_money, buy_spirit, buy_recycle_point, sell_recycle_point, sell_money, category, unlock, no_steam, platform)
SELECT 'SKL_FTYPE_DEF', (SELECT MAX(no) + 1 FROM master_skill), display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, 1, 0, 0, start, end, rarity, battle_rate, 0, 0, 0, 0, 0, category, unlock, 0, 0
FROM master_skill WHERE id = '{{def_skill}}';

INSERT OR REPLACE INTO master_skill (id, no, display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, is_nosale, is_display, is_display_list, start, end, rarity, battle_rate, buy_money, buy_spirit, buy_recycle_point, sell_recycle_point, sell_money, category, unlock, no_steam, platform)
SELECT 'SKL_FMAP_DEF', (SELECT MAX(no) + 1 FROM master_skill), display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, 1, 0, 0, start, end, rarity, battle_rate, 0, 0, 0, 0, 0, category, unlock, 0, 0
FROM master_skill WHERE id = 'SKL_SEARCHUP_ITEM_P' AND {{def_map}} = 1;

-- Say so in the fighter shop (English text only).
UPDATE master_text
SET txt = txt || ' Passive: '
    || CASE WHEN '{{def_skill}}' <> '' THEN COALESCE((
           SELECT t.txt FROM master_skill s JOIN master_text t
             ON t.sct = 'SKILL_NAME' AND 'SKILL_NAME.' || t.id = s.name AND t.lang = 'int'
           WHERE s.id = '{{def_skill}}'), '{{def_skill}}') ELSE '' END
    || CASE WHEN '{{def_skill}}' <> '' AND {{def_map}} = 1 THEN ' + ' ELSE '' END
    || CASE WHEN {{def_map}} = 1 THEN 'map reveal' ELSE '' END
    || '.'
WHERE sct = 'FTYPE_DSC' AND id = 'TXT_DEF' AND lang = 'int'
  AND ('{{def_skill}}' <> '' OR {{def_map}} = 1);

-- Shooter (SHT) --------------------------------------------------

INSERT OR REPLACE INTO master_skill (id, no, display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, is_nosale, is_display, is_display_list, start, end, rarity, battle_rate, buy_money, buy_spirit, buy_recycle_point, sell_recycle_point, sell_money, category, unlock, no_steam, platform)
SELECT 'SKL_FTYPE_SHT', (SELECT MAX(no) + 1 FROM master_skill), display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, 1, 0, 0, start, end, rarity, battle_rate, 0, 0, 0, 0, 0, category, unlock, 0, 0
FROM master_skill WHERE id = '{{sht_skill}}';

INSERT OR REPLACE INTO master_skill (id, no, display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, is_nosale, is_display, is_display_list, start, end, rarity, battle_rate, buy_money, buy_spirit, buy_recycle_point, sell_recycle_point, sell_money, category, unlock, no_steam, platform)
SELECT 'SKL_FMAP_SHT', (SELECT MAX(no) + 1 FROM master_skill), display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, 1, 0, 0, start, end, rarity, battle_rate, 0, 0, 0, 0, 0, category, unlock, 0, 0
FROM master_skill WHERE id = 'SKL_SEARCHUP_ITEM_P' AND {{sht_map}} = 1;

-- Say so in the fighter shop (English text only).
UPDATE master_text
SET txt = txt || ' Passive: '
    || CASE WHEN '{{sht_skill}}' <> '' THEN COALESCE((
           SELECT t.txt FROM master_skill s JOIN master_text t
             ON t.sct = 'SKILL_NAME' AND 'SKILL_NAME.' || t.id = s.name AND t.lang = 'int'
           WHERE s.id = '{{sht_skill}}'), '{{sht_skill}}') ELSE '' END
    || CASE WHEN '{{sht_skill}}' <> '' AND {{sht_map}} = 1 THEN ' + ' ELSE '' END
    || CASE WHEN {{sht_map}} = 1 THEN 'map reveal' ELSE '' END
    || '.'
WHERE sct = 'FTYPE_DSC' AND id = 'TXT_SHT' AND lang = 'int'
  AND ('{{sht_skill}}' <> '' OR {{sht_map}} = 1);

-- Lucky Star (LUK) -----------------------------------------------

INSERT OR REPLACE INTO master_skill (id, no, display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, is_nosale, is_display, is_display_list, start, end, rarity, battle_rate, buy_money, buy_spirit, buy_recycle_point, sell_recycle_point, sell_money, category, unlock, no_steam, platform)
SELECT 'SKL_FTYPE_LUK', (SELECT MAX(no) + 1 FROM master_skill), display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, 1, 0, 0, start, end, rarity, battle_rate, 0, 0, 0, 0, 0, category, unlock, 0, 0
FROM master_skill WHERE id = '{{luk_skill}}';

INSERT OR REPLACE INTO master_skill (id, no, display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, is_nosale, is_display, is_display_list, start, end, rarity, battle_rate, buy_money, buy_spirit, buy_recycle_point, sell_recycle_point, sell_money, category, unlock, no_steam, platform)
SELECT 'SKL_FMAP_LUK', (SELECT MAX(no) + 1 FROM master_skill), display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, 1, 0, 0, start, end, rarity, battle_rate, 0, 0, 0, 0, 0, category, unlock, 0, 0
FROM master_skill WHERE id = 'SKL_SEARCHUP_ITEM_P' AND {{luk_map}} = 1;

-- Say so in the fighter shop (English text only).
UPDATE master_text
SET txt = txt || ' Passive: '
    || CASE WHEN '{{luk_skill}}' <> '' THEN COALESCE((
           SELECT t.txt FROM master_skill s JOIN master_text t
             ON t.sct = 'SKILL_NAME' AND 'SKILL_NAME.' || t.id = s.name AND t.lang = 'int'
           WHERE s.id = '{{luk_skill}}'), '{{luk_skill}}') ELSE '' END
    || CASE WHEN '{{luk_skill}}' <> '' AND {{luk_map}} = 1 THEN ' + ' ELSE '' END
    || CASE WHEN {{luk_map}} = 1 THEN 'map reveal' ELSE '' END
    || '.'
WHERE sct = 'FTYPE_DSC' AND id = 'TXT_LUC' AND lang = 'int'
  AND ('{{luk_skill}}' <> '' OR {{luk_map}} = 1);

-- Skill Master (SKI) ---------------------------------------------

INSERT OR REPLACE INTO master_skill (id, no, display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, is_nosale, is_display, is_display_list, start, end, rarity, battle_rate, buy_money, buy_spirit, buy_recycle_point, sell_recycle_point, sell_money, category, unlock, no_steam, platform)
SELECT 'SKL_FTYPE_SKI', (SELECT MAX(no) + 1 FROM master_skill), display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, 1, 0, 0, start, end, rarity, battle_rate, 0, 0, 0, 0, 0, category, unlock, 0, 0
FROM master_skill WHERE id = '{{ski_skill}}';

INSERT OR REPLACE INTO master_skill (id, no, display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, is_nosale, is_display, is_display_list, start, end, rarity, battle_rate, buy_money, buy_spirit, buy_recycle_point, sell_recycle_point, sell_money, category, unlock, no_steam, platform)
SELECT 'SKL_FMAP_SKI', (SELECT MAX(no) + 1 FROM master_skill), display_label, name, type, desc, getroute, exp, val0, val1, val2, val3, val4, val5, premium, 1, 0, 0, start, end, rarity, battle_rate, 0, 0, 0, 0, 0, category, unlock, 0, 0
FROM master_skill WHERE id = 'SKL_SEARCHUP_ITEM_P' AND {{ski_map}} = 1;

-- Say so in the fighter shop (English text only).
UPDATE master_text
SET txt = txt || ' Passive: '
    || CASE WHEN '{{ski_skill}}' <> '' THEN COALESCE((
           SELECT t.txt FROM master_skill s JOIN master_text t
             ON t.sct = 'SKILL_NAME' AND 'SKILL_NAME.' || t.id = s.name AND t.lang = 'int'
           WHERE s.id = '{{ski_skill}}'), '{{ski_skill}}') ELSE '' END
    || CASE WHEN '{{ski_skill}}' <> '' AND {{ski_map}} = 1 THEN ' + ' ELSE '' END
    || CASE WHEN {{ski_map}} = 1 THEN 'map reveal' ELSE '' END
    || '.'
WHERE sct = 'FTYPE_DSC' AND id = 'TXT_SKI' AND lang = 'int'
  AND ('{{ski_skill}}' <> '' OR {{ski_map}} = 1);

-- The Steam game wants a Steam number on every row it loads.
UPDATE master_skill SET no_steam = no
WHERE id GLOB 'SKL_FTYPE_*' OR id GLOB 'SKL_FMAP_*';
