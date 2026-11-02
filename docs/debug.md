## Introduction

If you compile the game in debug, you have multiple debug features available.

## Lua console

Press **`` ` ``** (backtick) in-game to open the Lua console. All debug commands are run from there.
Tab-completion is available — type `dev.` and press Tab to browse available modules.

### Common commands

Tip: Debug commands are also supported in quick messages. If you need a debug command frequently, put it in a quick message. :wink:

| Command                         | Description                                                                         |
| ------------------------------- | ----------------------------------------------------------------------------------- |
| `dev.player.god()`              | Toggle god mode (invincible, infinite mana). Pass `true`/`false` to set explicitly. |
| `dev.player.invisible()`        | Toggle invisibility (monsters ignore you).                                          |
| `dev.player.gold.give()`        | Fill inventory with gold. Pass an amount to give a specific value.                  |
| `dev.player.gold.take()`        | Remove all gold. Pass an amount to remove a specific value.                         |
| `dev.player.info()`             | Print player position, HP, mode, and other state.                                   |
| `dev.items.spawn("name")`       | Spawn an item by partial name match.                                                |
| `dev.items.spawnUnique("name")` | Spawn a unique item by partial name match.                                          |

## Chat commands

The chat box is only available in **multiplayer** (Enter opens it). It accepts a few commands:

| Command           | Description                                                 |
| ----------------- | ----------------------------------------------------------- |
| `/help`           | Shows a list of all available chat commands.                |
| `/seedinfo`       | Show the seed info for the current level.                   |
| `/inspect <name>` | Inspect another player's stats and equipment (multiplayer). |

## Command-line parameters

| Command | Description                                                                                                     |
| ------- | --------------------------------------------------------------------------------------------------------------- |
| `+`     | Executes a debug command when loading the first game. For example `+god` or `+changelevel 1 +spawn 4 skeleton`. |
| `-f`    | Display frames per second.                                                                                      |
| `-i`    | Disable network timeout.                                                                                        |
| `-n`    | Disable startup video.                                                                                          |

## In-game hotkeys

| Hotkey  | Description                                                                                                     |
| ------- | --------------------------------------------------------------------------------------------------------------- |
| `Shift` | While holding, you can use the mouse to scroll screen.                                                          |
| `m`     | Print debug monster info.                                                                                       |
| `M`     | Switch current debug monster.                                                                                   |
| `` ` `` | Open the Lua console.                                                                                           |
| `x`     | Toggles `DebugToggle` variable. `DebugToggle` is a generic solution for temporary toggles needed for debugging. |
