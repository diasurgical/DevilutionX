# Player class data

There is one folder per class.

### attributes.tsv

 Attribute         | Description
------------------:|--------------------------------------
 `baseStr`              | Class Starting Strength Stat, uint8_t
 `baseMag`              | Class Starting Magic Stat, uint8_t
 `baseDex`              | Class Starting Dexterity Stat, uint8_t
 `baseVit`              | Class Starting Vitality Stat, uint8_t
 `maxStr`               | Class Maximum Strength Stat, uint8_t
 `maxMag`               | Class Maximum Magic Stat, uint8_t
 `maxDex`               | Class Maximum Dexterity Stat, uint8_t
 `maxVit`               | Class Maximum Vitality Stat, uint8_t
 `blockBonus`           | Class Block Bonus, %
 `adjLife`              | Class Life Adjustment, decimal
 `adjMana`              | Class Mana Adjustment, decimal
 `lvlLife`              | Life gained on level up, decimal
 `lvlMana`              | Mana gained on level up, decimal
 `chrLife`              | Life from base Vitality, decimal
 `chrMana`              | Mana from base Magic, decimal
 `itmLife`              | Life from item bonus Vitality, decimal
 `itmMana`              | Mana from item bonus Magic, decimal
 `manaCost`             | Mana cost for spells multipler, decimal
 `itmRestoreLife`       | Life restored by items multipler, decimal
 `itmRestoreMana`       | Mana restored by items multipler, decimal
 `splRestoreLife`       | Life restored by spells multipler, decimal
 `splRestoreMana`       | Mana restored by spells multiplerr, decimal
 `healOtherRestoreLife` | Life restored by spells Heal Other spell multipler, decimal
 `baseMagicToHit`       | Starting chance to hit with spells/scrolls, %
 `baseMeleeToHit`       | Starting chance to hit with melee weapons/fists, %
 `baseRangedToHit`      | Starting chance to hit with ranged weapons, %
 
 ### sprites.tsv

 Attribute         | Description
------------------:|--------------------------------------
 `classPath`           | Path inside the "plrgfx" folder where this player class graphics are stored, string
 `classChar`           | Prefix character used int the name of the stance subfolder and the player graphics .cl2 files, char
 `trn`                 | Path of the palette translation file to apply for this player class graphics, string
 `partyOffsetTownX`    | Offset X for the player portrait in town level, int8_t
 `partyOffsetTownY`    | Offset Y for the player portrait in town level, int8_t
 `partyOffsetDungeonX` | Offset X for the player portrait in dungeon level, int8_t
 `partyOffsetDungeonY` | Offset Y for the player portrait in dungeon level, int8_t
 `partyOffsetDeadX`    | Offset X for the player portrait when dead, int8_t
 `partyOffsetDeadY`    | Offset Y for the player portrait when dead, int8_t
 `stand`               | Width of the stand stance sprites, uint8_t
 `walk`                | Width of the walk stance sprites, uint8_t
 `attack`              | Width of the melee attack stance sprites, uint8_t
 `bow`                 | Width of the bow attack sprites, uint8_t
 `swHit`               | Width of the hit recovery stance sprites, uint8_t
 `block`               | Width of the block stance sprites, uint8_t
 `lightning`           | Width of the cast lightning spell stance sprites, uint8_t
 `fire`                | Width of the cast fire spell stance sprites, uint8_t
 `magic`               | Width of the cast magic spell stance sprites, uint8_t
 `death`               | Width of the death stance sprites, uint8_t
 