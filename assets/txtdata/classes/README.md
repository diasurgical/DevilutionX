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
