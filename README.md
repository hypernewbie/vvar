# vvar

A modern C++17 port of the Quake III Arena cvar and command system, extracted as a lightweight, header-friendly library. If you know what a cvar is, you know what this does.

## What's included

- **`veCVar`** — Console variables with full flag support (archive, latch, ROM, cheat-protected, userinfo/serverinfo/systeminfo propagation, range validation)
- **`veCmd`** — Command buffer with tokenization, deferred execution (`wait`), glob filtering, and tab-completion hooks
- **`veIVar`** — Key/value info strings with Q3-style `\key\value` serialization/parsing helpers
- Default commands wired up via `veCmd_InitDefaultFunctions()`: `alias`, `exec`, `set`, `seta`, `setu`, `sets`, `reset`, `toggle`, `print`, `cvarlist`, `cvar_modified`, `cvar_restart`, `echo`, `wait`, `cmdlist`

## Requirements

- C++17
- CMake 3.20+
- The header provides default `dinfo(...)`, `derr(...)`, and `derr_fatal(...)` fallbacks that write to `stderr` and abort on fatal errors
- To override logging, define `dinfo`, `derr`, and `derr_fatal` macros before including `vvar.h`, or define `VVAR_NO_DEFAULT_LOGGING` and provide your own implementations

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

If you want custom logging, define your hooks before including the header:

```cpp
#define dinfo(...) my_info_logger(__VA_ARGS__)
#define derr(...) my_error_logger(__VA_ARGS__)
#define derr_fatal(...) my_fatal_logger(__VA_ARGS__)
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

### Lazy cvar refs

`veCVarRef` resolves on first use and caches the pointer after that:

```cpp
veCVarRef r_mode("r_mode", "3", VE_CVAR_ARCHIVE | VE_CVAR_LATCH);
int mode = r_mode->getInteger();
```

Call `veCVar::init()` before first dereference. If you resolve a ref before `init()`, `init()` will clear the table and invalidate the cached pointer.

### Register and dispatch commands

```cpp
veCmd_InitDefaultFunctions(); // wire up built-ins

veGetCmd().addCommand("quit", []() {
    // handle quit
});

veGetCmd().executeString("set r_fullscreen 0; quit");
veGetCmd().execute(VE_CMD_EXEC_NOW);
```

Aliases are expanded before the command table, so Q3-style command chains work as expected:

```cpp
veGetCmd().executeString("alias jump \"+moveup; wait; -moveup\"");
veGetCmd().executeString("jump");
```

`exec <filename>` reads a script file and inserts it into the command buffer.

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

### Info strings

`veIVar` stores named info-string maps and can round-trip the Quake-style wire format:

```cpp
veIVar::fromString("userinfo", "\\name\\Player\\rate\\25000");
const char* info = veIVar::toString("userinfo");
```

### Direct numeric writes

`VE_CVAR_ALLOW_SET_INTEGER` opts a cvar into explicit numeric back-buffer sync. If you mutate a cvar through `getInteger()` or `getValue()`, call `veCVar::updateFromIntegerFloatValues()` once per frame (or at your chosen sync point) to push those numeric changes back into the string form.

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
| `VE_CVAR_ALLOW_SET_INTEGER` | Allows explicit `getInteger()` / `getValue()` writes to be synchronized back into the string form via `updateFromIntegerFloatValues()` |

## Notes

- `reset` respects normal cvar protections and latch behavior; `forceReset` bypasses them.
- `veq3_va()` now uses a thread-local ring buffer, so it is safe across threads and still supports a small amount of nesting.

## License

Derived from Quake III Arena source code, copyright 1999–2005 Id Software, Inc. Licensed under the GNU General Public License v2 (or later). See `LICENSE` for the full terms.
