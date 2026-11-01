# Shrines data

All the random shrines data is contained inside shrines.tsv

### shrines.tsv

 Attribute                     | Description
------------------------------:|-------------------------------------------------------------------------------------------
 `id`                          | Shrine id (unused)
 `name`                        | Shrine name shown when mouse hover, string
 `operateFn`                   | Function that will operate upon clicking shrine, void* function
 `powerParam1`                 | First parameter that configures shrine effect, int
 `powerParam2`                 | Second parameter that configures shrine effect, int
 `gameType`                    | Single player, multiplayer, or both, enum
 `minLevelType`                | Minimum dungeon type where the shrine is available, dungeon_type
 `maxLevelType`                | Maximum dungeon type where the shrine is available, dungeon_type
 `availableRandomEffectShrine` | Shrine is available when using random effect shrines (goat shrines and cauldrons), boolean
 `cripplingEffect`             | Shrine is marked as crippling, boolean
 `message`                     | Standar message shown when messageIndex is 1, string
 `message2`                    | Alternate message shown when messageIndex is 2 (other players and solar shrine), string
 `message3`                    | Alternate message shown when messageIndex is 3 (solar shrine), string
 `message4`                    | Alternate message  shown when messageIndex is 4 (solar shrine), string

### Available values

`gameType enum`

 gameType           | Value
-------------------:|----------------------------
 `ShrineTypeAny`    | Both single and multiplayer
 `ShrineTypeSingle` | Only single player
 `ShrineTypeMulti`  | Only multiplayer

`dungeon_type enum`

 dungeon_type      | Value
------------------:|----------
 `DTYPE_NONE`      | None
 `DTYPE_TOWN`      | Town
 `DTYPE_CATHEDRAL` | Cathedral
 `DTYPE_CATACOMBS` | Catacombs
 `DTYPE_CAVES`     | Caves
 `DTYPE_HELL`      | Hell
 `DTYPE_NEST`      | Nest
 `DTYPE_CRYPT`     | Crypt

`operateFn functions`

 operateFn                   | Description                                                                                                                                                | powerParam1                                                    | powerParam2                                                                                            | messageIndex                        
----------------------------:|------------------------------------------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------|--------------------------------------------------------------------------------------------------------|---------------------------------------------------
 `OperateShrineMysterious`   | Modifies a random attribute using first value and the rest with the second value                                                                           | Value to change a random attribute                             | Value to change the rest of the attributes                                                             | 1 player operating shrine, 0 otherwise 
 `OperateShrineHidden`       | Modifies the durability of a random item using first value and the rest of items with the second value                                                     | Durability change to a random item                             | Durability change for the rest of the items                                                            | 1 player operating shrine, 0 otherwise 
 `OperateShrineGloomy`       | For all items in the player's inventory, modifies the armor class of all armor and the max damage of all weapons                                           | Change to items armor class                                    | Change to items max damage                                                                             | 1 player operating shrine, 0 otherwise 
 `OperateShrineWeird`        | For all items in the player's inventory, modifies the max damage of all weapons                                                                            | Change to items max damage                                     | Unused                                                                                                 | 1 player operating shrine, 0 otherwise 
 `OperateShrineMagical`      | Casts a mana shield                                                                                                                                        | Spell level multiplier for dungeon level                       | Spell level add to dungeon level                                                                       | 1 player operating shrine, 0 otherwise 
 `OperateShrineStone`        | Recharges all staves in the player's inventory                                                                                                             | Unused                                                         | Unused                                                                                                 | 1 player operating shrine, 0 otherwise 
 `OperateShrineReligious`    | Repairs all items in the player's inventory                                                                                                                | Unused                                                         | Unused                                                                                                 | 1 player operating shrine, 0 otherwise 
 `OperateShrineEnchanted`    | Modifies the level of a random spell using first value and the rest of spell levels with the second value                                                  | Level change to a random spell                                 | Level change for the rest of the spells                                                                | 1 player operating shrine, 0 otherwise 
 `OperateShrineThaumaturgic` | Closes and refills all opened chests on current level                                                                                                      | Unused                                                         | Unused                                                                                                 | 1 player operating shrine, 0 otherwise 
 `OperateShrineFascinating`  | Increases Firebolt spell level and reduces max mana (or magic if loss is negative)                                                                         | Amount of levels to increase the Firebolt spell                | Percent of max mana to permanently reduce (when positive) or magic attribute to reduce (when negative) | 1 player operating shrine, 0 otherwise 
 `OperateShrineCryptic`      | Casts a nova spell and refills your mana                                                                                                                   | Spell level multiplier for dungeon level                       | Spell level add to dungeon level                                                                       | 1 player operating shrine, 0 otherwise 
 `OperateShrineEldritch`     | All potions in the player's inventory are turn to Rejuvenation                                                                                             | Unused                                                         | Unused                                                                                                 | 1 player operating shrine, 0 otherwise 
 `OperateShrineEerie`        | Increases the magic attribute                                                                                                                              | Amount to increase the magic attribute                         | Unused                                                                                                 | 1 player operating shrine, 0 otherwise 
 `OperateShrineDivine`       | Fully restores HP and Mana of the active player and spawns potions                                                                                         | The minimum dungeon level that will spawn rejuvenation potions | The amoun of potions                                                                                   | 1 player operating shrine, 0 otherwise 
 `OperateShrineHoly`         | Casts a phasing spell                                                                                                                                      | Spell level multiplier for dungeon level                       | Spell level add to dungeon level                                                                       | 1 player operating shrine, 0 otherwise 
 `OperateShrineSacred`       | Increases Charged Bolt spell level and reduces max mana (or magic if loss is negative)                                                                     | Amount of levels to increase the Charged Bolt spell            | Percent of max mana to permanently reduce (when positive) or magic attribute to reduce (when negative) | 1 player operating shrine, 0 otherwise 
 `OperateShrineSpiritual`    | Fill empty slots in inventory with gold based in the dungeon level                                                                                         | First multiplier of gold per dungeon level                     | Second multiplier of gold per dungeon level                                                            | 1 player operating shrine, 0 otherwise 
 `OperateShrineSpooky`       | No effect for the player who activated the shrine, the other players gets its life and mana fully restored                                                 | Unused                                                         | Unused                                                                                                 | 1 player operating shrine, 2 for the other players  
 `OperateShrineAbandoned`    | Increases the dexterity attribute                                                                                                                          | Amount to increase the dexterity attribute                     | Unused                                                                                                 | 1 player operating shrine, 0 otherwise 
 `OperateShrineCreepy`       | Increases the strength attribute                                                                                                                           | Amount to increase the strength attribute                      | Unused                                                                                                 | 1 player operating shrine, 0 otherwise
 `OperateShrineQuiet`        | Increases the vitality attribute                                                                                                                           | Amount to increase the vitality attribute                      | Unused                                                                                                 | 1 player operating shrine, 0 otherwise
 `OperateShrineSecluded`     | Reveals all the current level minimap                                                                                                                      | Unused                                                         | Unused                                                                                                 | 1 player operating shrine, 0 otherwise
 `OperateShrineOrnate`       | Increases Holy Bolt spell level and reduces max mana (or magic if loss is negative)                                                                        | Amount of levels to increase the Holy Bolt spell               | Percent of max mana to permanently reduce (when positive) or magic attribute to reduce (when negative) | 1 player operating shrine, 0 otherwise
 `OperateShrineGlimmering`   | Identifies all items in the player’s inventory                                                                                                             | Unused                                                         | Unused                                                                                                 | 1 player operating shrine, 0 otherwise
 `OperateShrineTainted`      | No effect for the player who activated the shrine, the other players gets a random attribute modified using first value and the rest with the second value | Value to change a random attribute                             | Value to change the rest of the attributes                                                             | 1 player operating shrine, 2 for the other players
 `OperateShrineOily`         | Oily shrines increase the players primary stat(s), but spawn a firewall near the shrine that will spread towards the player                                | Value to change primary stats                                  | Value of how much will be given to the second primary stat, substracted from the first                 | 1 player operating shrine, 0 otherwise
 `OperateShrineGlowing`      | Increase magic attribute but reduces total experience                                                                                                      | Value to increase magic attribute                              | Fraction of experience lost                                                                            | 1 player operating shrine, 0 otherwise
 `OperateShrineMendicant`    | Takes a percent of the player's gold and converts it to experience                                                                                         | % of gold consumed                                             | Experience gained for each gold unit consumed                                                          | 1 player operating shrine, 0 otherwise
 `OperateShrineSparkling`    | Grants experience to the player based on the current dungeon level while also triggering a magic trap                                                      | Exp multiplier per dungeon level                               | Flash spell damage multiplier per dungeon level                                                        | 1 player operating shrine, 0 otherwise
 `OperateShrineTown`         | Spawns a town portal near the active player                                                                                                                | Unused                                                         | Unused                                                                                                 | 1 player operating shrine, 0 otherwise
 `OperateShrineShimmering`   | Replenishes the mana to maximum of the player who activated it                                                                                             | Unused                                                         | Unused                                                                                                 | 1 player operating shrine, 0 otherwise
 `OperateShrineSolar`        | Increases an attribute based on the current hour                                                                                                           | Amount to increase the attribute                               | Unused                                                                                                 | 1 - 4 depending on the effect for the player operating shrine, 0 otherwise
 `OperateShrineMurphy`       | Decreses an item durability or if none selected, takes away a fraction of gold                                                                             | % of durability to decrease                                    | Fraction of gold to take away if no item is selected                                                   | 1 player operating shrine, 0 otherwise

When messageIndex is outside values 1 to 4, no message is shown 