# Zed configuration

`settings.json` is the project configuration, copied exactly from the current
Windows Zed user settings on 2026-10-13. Zed loads supported project settings
from this file when the repository is opened. Settings supported only at user
scope still need the user configuration; this file does not install extensions
or authenticate agent providers.

## User configuration backup

Snapshot saved on 2026-10-13 from the current Windows Zed user configuration.

| Repository copy | Original location |
| --- | --- |
| `user-config/settings.json` | `%APPDATA%/Zed/settings.json` |
| `user-config/AGENTS.md` | `%APPDATA%/Zed/AGENTS.md` |

These are exact backup copies. `settings.json` uses Zed's commented JSON syntax
and includes the current theme, font sizes, panel layout and agent preferences.
`AGENTS.md` contains the user's commit-message instructions.

To restore the snapshot, copy the files back to the corresponding Zed user
configuration locations after backing up any newer settings. The copies are
stored below `user-config/` rather than as active project settings. Repository
agent policy remains in the root `AGENTS.md`.

The user themes directory contained no files. Runtime caches, installed
extensions, logs, databases and conversation history are outside this snapshot.
