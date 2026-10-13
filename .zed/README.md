# Zed configuration

`settings.json` is the project configuration, initially copied from the current
Windows Zed user settings on 2026-10-13, with C/C++ formatting on save added.
Zed loads supported project settings
from this file when the repository is opened. Settings supported only at user
scope still need the user configuration; this file does not install extensions
or authenticate agent providers.

## Build and run tests

Ctrl+F5 opens Zed's task picker with these four project actions:

1. **Build Unit Test**
2. **Launch Unit Test**
3. **Build Executable**
4. **Launch Executable**

The tasks save edited buffers and call `tools/zed_task.py`. Python runs each
command directly, without `&&` or shell-dependent command parsing, and stops on
failure. All four actions use exceptions OFF. Unit actions use
`debug-unit-tests`; launch runs CTest with verbose output. Configure that preset
first using the root README commands. Exe actions use `debug-example`, also with
exceptions OFF. The helper does not offer an exception-mode override.

CTest registers each individual GoogleTest case, including parameterized cases.
The final summary counts cases rather than executables (currently 98 cases).

**Launch Unit Test** and **Launch Executable** first ask CMake's native build system
to update their targets. Changed sources, included headers or build files trigger
the necessary compilation; unchanged targets are launched without recompiling.
This also rebuilds a missing binary. Build actions use incremental builds too;
no action forces an unnecessary full rebuild.

The example executable is not implemented yet. Both executable actions report this
and exit unsuccessfully until `examples/main.cpp` and the parser target exist.
Once implemented, they use the `debug-example` preset and
`adaptive_json5_examples` target; the launch action starts the built executable.

Each action reuses its own terminal tab on subsequent runs. The four separate
actions do not share one tab. `use_new_terminal` and `allow_concurrent_runs`
remain false. Old tabs from earlier configurations can be closed manually.

The shortcut is installed in Zed's user `keymap.json` on the current machine.
`.zed/keymap.json` is the repository copy; Zed does not automatically import it
as a project keymap. On another machine, merge its entry through **zed: open
keymap**. Ctrl+Shift+R also opens the task picker.

See [Zed task keybindings](https://zed.dev/docs/tasks#custom-keybindings-for-tasks).

## Formatting on save

C and C++ buffers are formatted on save using `tools/zed_format.py`. The project
settings launch Python and locate the helper from the edited buffer's absolute
path, so the checkout can move and Zed's working directory does not matter.
The helper selects the repository's `.clang-format` and finds clang-format
18.1.8 in local build storage or on PATH. Windows, Linux and macOS executable
names are handled automatically. No machine-specific formatter paths are tracked.

Python 3.9+ must be available to Zed as `python`. If your system provides only
`python3`, install/configure a `python` launcher or change the formatter command
to `python3` in your local editor settings. Prepare the pinned local formatter
from the repository root on each machine:

```sh
python -m pip install --target build/format-tools clang-format==18.1.8
```

Deleting the build directory removes this local tool; repeat the install command.
Alternatively, install clang-format 18.1.8 on PATH. An unsupported formatter
version produces an explicit error instead of silently changing the style.

The helper receives the unsaved buffer on stdin and returns formatted text on
stdout. It neither reads nor rewrites the edited file. `{buffer_path}` identifies
the checkout and supplies the assumed filename. No in-place formatter flag is
used. The original user-settings backup remains under `user-config/`.

See [Zed's formatter settings](https://zed.dev/docs/reference/all-settings#formatter).

## C++ completion and standard headers

The root `.clangd` selects `build/debug-unit-tests/compile_commands.json` and
C++17. Configure the unit-test preset with your native toolchain to generate
that database. Zed's clangd arguments allow querying the Windows Scoop GCC
compiler used by this checkout for its target and standard-library include
paths. Both Scoop `gcc` and `mingw` compiler paths are allowed, since the
current compilation database uses `mingw`. This is editor configuration,
not a parser dependency or quality gate.
Other toolchains may need their own narrowly scoped `--query-driver` argument
in user settings. See [clangd system headers](https://clangd.llvm.org/guides/system-headers).

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
