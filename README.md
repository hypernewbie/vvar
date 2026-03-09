# vvar

A modern C++17 port of the Quake III Arena cvar and command system, extracted as a lightweight, header-friendly library. If you know what a cvar is, you know what this does.

## What's included

- **`veCVar`** — Console variables with full flag support (archive, latch, ROM, cheat-protected, userinfo/serverinfo/systeminfo propagation, range validation)
- **`veCmd`** — Command buffer with tokenization, deferred execution (`wait`), glob filtering, and tab-completion hooks
- **`veIVar`** — Key/value info strings (flat nested map, no parsing overhead)
- Default commands wired up via `veCmd_InitDefaultFunctions()`: `set`, `seta`, `setu`, `sets`, `reset`, `toggle`, `print`, `cvarlist`, `cvar_modified`, `cvar_restart`, `echo`, `wait`, `cmdlist`

## Requirements

- C++17
- CMake 3.20+
- Your project must define `dinfo(...)`, `derr(...)`, and `derr_fatal(...)` — the library uses these for logging and fatal errors but does not provide implementations

## Build

```bash
cmake -B build
cmake --build build
ctest --test-dir build
```

Ninja and clang-cl are both supported.

## Usage

Include `vvar_impl.h` in **one** translation unit. Include `vvar.h` everywhere else.

```cpp
// In one .cpp file only:
#include "vvar_impl.h"

// Elsewhere:
#include "vvar.h"
```

### Register and use a cvar

```cpp
// Register (creates if absent, ORs flags if already exists)
veCVar* r_fullscreen = veCVar::get("r_fullscreen", "1", VE_CVAR_ARCHIVE);

// Read
int fs = veCVar::variableIntegerValue("r_fullscreen");

// Write
veCVar::set("r_fullscreen", "0");
```

### Register and dispatch commands

```cpp
veCmd_InitDefaultFunctions(); // wire up built-ins

veGetCmd().addCommand("quit", []() {
    // handle quit
});

veGetCmd().executeString("set r_fullscreen 0; quit");
veGetCmd().execute(VE_CMD_EXEC_NOW);
```

### Latched cvars

Changes are staged and applied the next time `veCVar::get()` is called for that variable — useful for settings that require a restart to take effect.

```cpp
veCVar* vid_restart = veCVar::get("r_mode", "3", VE_CVAR_LATCH | VE_CVAR_ARCHIVE);
```

### Saving archived vars

```cpp
veFileData f;
veCVar::writeVariables(f); // emits "set <name> <value>" lines for all VE_CVAR_ARCHIVE vars
```

## Cvar flags

| Flag | Description |
|---|---|
| `VE_CVAR_ARCHIVE` | Saved to config on `writeVariables` |
| `VE_CVAR_LATCH` | Change takes effect on next `get()` call |
| `VE_CVAR_ROM` | Display only, not user-settable |
| `VE_CVAR_INIT` | Set from command line only |
| `VE_CVAR_CHEAT` | Locked when cheats are disabled |
| `VE_CVAR_PROTECTED` | Cannot be set from VMs or remote |
| `VE_CVAR_USERINFO` | Sent to server on connect/change |
| `VE_CVAR_SERVERINFO` | Sent in response to frontend requests |
| `VE_CVAR_SYSTEMINFO` | Replicated to all clients |

## License

Derived from Quake III Arena source code, copyright 1999–2005 Id Software, Inc. Licensed under the GNU General Public License v2 (or later). See `LICENSE` for the full terms.
