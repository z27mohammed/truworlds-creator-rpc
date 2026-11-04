# TruWorlds Creator Discord Rich Presence

Shows what you're doing in **TruWorlds Creator** on your Discord profile.

It switches to **Playtesting <world>** while you're in a playtest and clears itself when you close Creator.

> **Unofficial, fan-made.** Not made by, affiliated with or endorsed by TruWorlds. "TruWorlds" and its logo
> belong to their owners.

## Install (Windows)

1. Download `TruWorldsCreatorRPC.zip` from Releases and extract it somewhere it can stay, e.g.
   `Documents\TruWorldsCreatorRPC`.
2. Double-click **`install.cmd`**. It starts now and at every logon, with no window and no admin rights needed.
3. Open TruWorlds Creator. Your Discord status updates within ~15 seconds.

You need the Discord **desktop app** running; the browser version doesn't support Rich Presence. In Discord,
Settings → Activity Privacy → "Share your detected activities with others" must be on.

**Stop it:** `stop.cmd` (until next logon). **Remove it:** `uninstall.cmd`, then delete the folder.

## Settings

Edit `config.ini` in Notepad; changes apply within a poll, no restart needed.

| Setting | Default | What it does |
| --- | --- | --- |
| `editingText` / `playtestingText` / `pickingText` | `Editing {world}` ... | The top line. `{world}` is the world name. |
| `showOtherWorlds` | `true` | "Also open: ..." when several Creator windows are open. |
| `hiddenWorlds` | empty | World names to keep private, comma-separated: `Secret Game, Other Game`. Shown as `hiddenWorldName`. |
| `hideAllWorldNames` | `false` | Hide every world name. |
| `hiddenWorldName` | `a secret project` | What hidden worlds show as. |
| `timer` | `creator` | `creator` (since Creator opened), `world` (since this world opened), `activity` (since you last switched between editing and playtesting) or `off`. |
| `button1Label`, `button1Url` (and `button2...`) | empty | Up to 2 link buttons, e.g. `Play my game` + `https://...`. Others see them; Discord hides them from you. |
| `clientId` | the TruWorlds Creator app | Use your own Discord app instead (discord.com/developers/applications); its name becomes the "Playing ..." text. |
| `largeImageKey`, `largeImageText` | empty | An Art Asset key from your own Discord app. Empty shows the app icon. |
| `smallImageEditing`, `smallImagePlaytesting` | empty | Small status-badge Art Asset keys (your own app). |
| `pollSeconds` | `15` | How often it checks. |

## How it works

- It reads Creator's window titles (`<World> - TruWorlds Creator`, and `Player 1 - ...` during a playtest)
  straight from Windows every 15 seconds. A window's world is the first name its title shows after opening;
  script tabs that appear later are never shown. It never connects to, injects into or modifies Creator, so it can't break your worlds.
- It talks to Discord through Discord's local Rich Presence pipe. Nothing else leaves your PC, and it has no
  network code, telemetry or updater.
- A single ~270 KB C++ exe using about 1 MB of RAM, with no dependencies beyond Windows itself. Logs go to
  `presence.log` in its folder.

## Build from source

All of it is `presence.cpp` (plain Win32, no libraries). Install Clang via LLVM-MinGW
(`winget install MartinStorsjo.LLVM-MinGW.UCRT`), then run `build.cmd`.
`TruWorldsCreatorRPC.exe --once` prints what it would show and exits; `--stop` stops a running copy.

## Known limits

- Windows 10/11 only.
- If it starts mid-playtest in a window it has never seen, it shows "Playtesting a world" until the playtest ends.
- The exe isn't code-signed, so Windows SmartScreen may warn the first time ("More info" → "Run anyway").
  Building it yourself from `presence.cpp` avoids that.

## License

Public domain (Unlicense). Do whatever you want with it.
