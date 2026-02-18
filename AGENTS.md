# AGENTS.md - HexChat Development Guide

This document provides guidelines for AI coding agents working on the HexChat codebase.

## Project Overview

HexChat is an IRC client written in C (gnu89 standard) using GTK for the GUI. It supports plugins in C, Python, Perl, and Lua.

## Build System

### Primary: Meson (>= 0.47.0)

```bash
meson build [options]      # Configure
ninja -C build             # Build
ninja -C build test        # Run tests
ninja -C build install     # Install
```

### Common Build Options

```bash
meson build -Dgtk-frontend=true -Dtls=enabled -Dplugin=true \
  -Dwith-python=python3 -Dwith-perl=perl -Dwith-lua=luajit -Dwith-fishlim=true
```

### Running a Single Test

Tests use TAP protocol. Currently only the fishlim plugin has tests:

```bash
ninja -C build test                              # Run all tests
./build/plugins/fishlim/tests/fishlim_tests      # Run fishlim tests directly
```

### Windows Build (Visual Studio)

```bash
msbuild win32/hexchat.sln /m /p:Configuration=Release /p:Platform=x64
```

## Code Style Guidelines

### Indentation and Formatting

- **C/C++/H files**: Tab indentation (size 4), LF line endings
- **Meson files**: Space indentation (size 2), final newline

### C Standard

The project uses **gnu89** C standard. Do not use C99/C11 features.

### Naming Conventions

| Element | Style | Example |
|---------|-------|---------|
| Functions | `snake_case` | `find_dialog`, `session_free` |
| Types/Structs | `snake_case` typedef | `session`, `server` |
| Macros/Constants | `UPPER_SNAKE_CASE` | `SESS_SERVER`, `NICKLEN` |
| Global variables | `snake_case` | `sess_list`, `plugin_list` |
| Preference vars | `hex_` prefix | `hex_away_auto_unmark` |
| Plugin API | `hexchat_` prefix | `hexchat_hook_command` |

### Header Guards

Use traditional `#ifndef/#define/#endif` pattern:

```c
#ifndef HEXCHAT_PLUGIN_H
#define HEXCHAT_PLUGIN_H
/* ... */
#endif
```

### Include Order

1. Standard library headers (`<stdio.h>`, `<string.h>`)
2. Platform-specific headers (`#ifdef WIN32`)
3. Project headers (`"hexchat.h"`, `"util.h"`)
4. Conditional feature headers (`#ifdef USE_OPENSSL`)

### License Header

All source files should include the GPL v2 license header:

```c
/* HexChat
 * Copyright (C) 1998-2010 Peter Zelezny.
 * Copyright (C) 2009-2013 Berke Viktor.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
```

## Error Handling

Use GLib assertion macros for precondition checks:

```c
g_return_if_fail(ptr != NULL);
g_return_val_if_fail(ptr != NULL, -1);
```

- Return `NULL` on error for pointer functions
- Return `-1` or error codes for integer functions
- Use `GError**` pattern for detailed error reporting

## Memory Management

Use GLib memory functions exclusively:

```c
ptr = g_new0(type, count);   /* Preferred: allocates zeroed memory */
ptr = g_malloc(size);
ptr = g_strdup(str);
ptr = g_strdup_printf("%s %d", str, num);
g_free(ptr);

GSList *list;  /* Single-linked list */
GList *list;   /* Double-linked list */
```

## Platform-Specific Code

```c
#ifdef WIN32
    /* Windows-specific code */
#else
    /* Unix/Linux code */
#endif
```

## Compiler Warnings (enforced as errors)

`-Werror=implicit-function-declaration`, `-Werror=pointer-arith`, `-Werror=init-self`,
`-Werror=format-security`, `-Werror=missing-include-dirs`, `-Werror=date-time`

## Directory Structure

```
hexchat/
├── src/
│   ├── common/      # Core IRC client logic (shared)
│   ├── fe-gtk/      # GTK frontend
│   └── fe-text/     # Text-mode frontend
├── plugins/
│   ├── fishlim/     # FiSH encryption (has tests)
│   ├── python/      # Python scripting
│   ├── perl/        # Perl scripting
│   └── lua/         # Lua scripting
├── data/            # Icons, man pages, desktop files
├── po/              # Translations (gettext)
└── win32/           # Windows build files
```

## Plugin Development

Plugins use the `hexchat_plugin` API defined in `src/common/hexchat-plugin.h`.

```c
int hexchat_plugin_init(hexchat_plugin *ph, char **name, char **desc, char **version, char *arg);
int hexchat_plugin_deinit(void);
```

## Testing

- Tests use TAP (Test Anything Protocol) format
- Test timeout: 600 seconds
- Test location: `plugins/*/tests/`

## CI/CD

GitHub Actions: `ubuntu-build.yml`, `windows-build.yml`, `msys-build.yml`, `flatpak-build.yml`

## Common Tasks

### Adding a new source file
1. Add to appropriate `meson.build` in the subdirectory
2. Include necessary headers
3. Add GPL license header

### Adding a new preference
1. Add to `struct hexchatprefs` in `src/common/hexchat.h`
2. Use `hex_` prefix for the variable name
3. Update preference loading/saving in `cfgfiles.c`

### Adding a new plugin
1. Create directory under `plugins/`
2. Add `meson.build` with sources and dependencies
3. Register in root `meson.build`

Use 'bd' for task tracking

## Landing the Plane (Session Completion)

**When ending a work session**, you MUST complete ALL steps below. Work is NOT complete until `git push` succeeds.

**MANDATORY WORKFLOW:**

1. **File issues for remaining work** - Create issues for anything that needs follow-up
2. **Run quality gates** (if code changed) - Tests, linters, builds
3. **Update issue status** - Close finished work, update in-progress items
4. **PUSH TO REMOTE** - This is MANDATORY:
   ```bash
   git pull --rebase
   bd sync
   git push
   git status  # MUST show "up to date with origin"
   ```
5. **Clean up** - Clear stashes, prune remote branches
6. **Verify** - All changes committed AND pushed
7. **Hand off** - Provide context for next session

**CRITICAL RULES:**
- Work is NOT complete until `git push` succeeds
- NEVER stop before pushing - that leaves work stranded locally
- NEVER say "ready to push when you are" - YOU must push
- If push fails, resolve and retry until it succeeds
