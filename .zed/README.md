# Zed configuration

`settings.json` is the project configuration, initially copied from the current
Windows Zed user settings on 2026-10-13, with C/C++ formatting on save added.
Zed loads supported project settings
from this file when the repository is opened. Settings supported only at user
scope still need the user configuration; this file does not install extensions
or authenticate agent providers.

## Formatting on save

C and C++ buffers are formatted on save by the external `clang-format` command.
Install the repository's required version, 18.1.8, and ensure it is on Zed's PATH:

```sh
python -m pip install clang-format==18.1.8
```

The formatter receives the buffer on standard input and returns formatted text
to Zed. `--assume-filename={buffer_path}` lets it find the repository's
`.clang-format` starting from the edited file's directory. No in-place formatter
flag is used. The original user-settings backup remains under `user-config/`.

See [Zed's formatter settings](https://zed.dev/docs/reference/all-settings#formatter).

## Header guard snippet

`snippets/c++.json` contains the `hguard` completion. Install it in Zed's user
snippets directory (`%APPDATA%/Zed/snippets/c++.json` on this Windows setup).
If that file already contains snippets, merge the `Header guard` entry instead
of replacing the file. Zed does not load this repository copy automatically.

In a C++ header, type `hguard` and accept the completion. Enter the uppercase
filename with dots replaced by underscores, for example `ERROR_HPP` for
`error.hpp`. The snippet adds the trailing underscore (`ERROR_HPP_`) to all
three linked occurrences. Tab moves to the header body.
Automatic filename variables are not currently supported by Zed's snippets;
see the [upstream feature request](https://github.com/zed-industries/zed/discussions/42248).
See [Zed snippets](https://zed.dev/docs/snippets).

## Codex agent preferences

`settings.json` preserves the current `agent_servers.codex-acp` entry with
registry type, default model `gpt-6.1-sol` and reasoning effort `low`.
The current agent-panel preferences are also preserved under `agent`.

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
