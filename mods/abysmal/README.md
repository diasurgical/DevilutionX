# Abysmal Diablo V2+ for DevilutionX

This port reconstructs Tommi Helminen's 6 November 1999 Abysmal V2+ rebalance
from the supplied executable, its Dview data dump and the original release
notes. It targets Diablo mode and requires the original Diablo game data.
It does not include Blizzard executables, graphics, audio or movies.

The original [Abysmal_V2+.txt](Abysmal_V2+.txt) is included unchanged, as its
author requested. Its legacy encoding is preserved.

## Activation

Build this checkout; an unmodified DevilutionX release does not implement all
of the restored effects and spell handlers. The build copies this directory to
`mods/abysmal` beside the executable (inside the application Resources directory
on macOS). For an existing installation, copy the
whole `abysmal` directory into its `mods` directory, either beside the executable
or in DevilutionX's preference directory.

Start the game in Diablo mode and enable **Abysmal Diablo V2+** in the Mods
settings. The installed mod is discovered automatically from its manifest.
Alternatively, with the game closed, add these entries to the `[Mods]` section
of `diablo.ini`:

```ini
[Mods]
abysmal=1
hf=0
```

Do not combine this rebalance with Hellfire or other gameplay mods. Create a
new character. The manifest selects `.asv` character/stash files and the `ABYS`
game identifier; ordinary Diablo `.sv` files remain separate. Original Abysmal
saves cannot be imported directly because the modern base-item indices differ.

Loose mod files retain DevilutionX's existing asset-integrity restrictions on
multiplayer. The save/game namespace does not certify identical content. A
packed MPQ is needed for the existing content-hash multiplayer compatibility
workflow; using the same mod name alone is insufficient.

## Behavior

The port restores the changed monster and unique-monster tables, 156 legacy
base-item records, 83 prefixes, 95 suffixes, 90 unique items, spell and missile
data, sound substitutions, starting attributes and attribute limits. Ancient
items occupy additional modern item slots so they do not overwrite Hellfire
or class-specific IDs. The original unique-item order is retained.

Warriors start with 35/0/15/30 Strength/Magic/Dexterity/Vitality, Rogues with
20/15/30/20 and Sorcerers with 5/45/15/0. Their respective attribute limits are
255/20/55/135, 65/75/255/65 and 20/255/115/20. Rogues start with Infravision and
Sorcerers with Identify. Other class properties retain their modern defaults.

The Butcher appears on dungeon level 3 and King Leoric on level 4. Adria and
Wirt move near the town portals. Resistance caps at 70%; ordinary gold stacks
hold 65,000. Vendor item-value limits, difficulty bonuses, Fireball tuning and
Chain Lightning range follow the recovered code. Wirt's two-million internal
item-value limit produces a three-million displayed price after his markup.
The automap starts enabled, and movement remains available during Diablo's
death animation.

Gloomy, Weird and Thaumaturgic shrines may appear in multiplayer. Fascinating,
Ornate and Sacred are disabled, including random goat-shrine and cauldron
results. Random goat/cauldron selection still excludes Thaumaturgic, as in the
existing game.

## Intent and compatibility

This port preserves the intended rebalance while retaining modern bug fixes.
Forced unique rewards on ordinary base items must reconstruct as the same item
after saving or transferring them. Infravision from equipment coexists with
timed spell vision, and successful weapon hits with the healing-prevention
power disable monster healing. Item art overrides use the supported cursor
field rather than a legacy pseudo-power.

Restored spells use a six-page spellbook with mouse and controller tabs. Matter
Trap waits on the casting tile and triggers once, using a 255-tick lifetime and
character-level dice damage derived from the original add handler. Escape creates
a return portal and enters town after the casting animation. Etherealize restores its
timed immunity and can be refreshed. Flameball launches three projectiles but
consumes one cast; its staff charges are adjusted to retain the original
number of usable casts. The `spell_overrides.tsv` table preserves the five
recovered icon substitutions and Flameball staff valuation while leaving
recharge costs unchanged. Doom Serpent uses valid sprite frame counts. FireMan
monsters regain their moving, reflecting charge and ranged attacks. These
implementations correct the original mismatched handlers, repeated mana/charge
consumption and invalid animation frames rather than reproduce those bugs.

The old vendor gold-pile splitting workaround and broken item descriptions
are not recreated. Gold remains bounded by the save format even if another
mode supplies a stack-doubling item. Modern random selection, input bindings
and item-generation fixes remain in use; this is not a byte-for-byte replay of
the old executable. High-level vendor affixes retain their original quality
levels. The legacy unused maximum-item-value field has no behavioral effect.

The original notes describe some behavior imprecisely. Recovered binary data
is authoritative where the two disagree; for example, the old data alone does
not impose an extra Hell-only restriction on Diablo's high-level unique pool.

Presentation overrides select the recovered startup palette, opening and ending
movies, bookshelf art and sound substitutions from the original game data. No
original game assets are redistributed. The original readme also lists
historical bugs; those are evidence about the old mod, not promised behavior
of this port.
