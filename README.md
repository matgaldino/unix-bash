# UNIX SHELL - biceps

`biceps` is a Unix shell written in C for an Operating Systems course with integrated BEUIP network protocol support. It includes:
- Interactive shell core (`biceps` + `gescom`)
- Reusable network/BEUIP library (`creme`)
- BEUIP protocol server/client (`servbeuip`, `clibeuip`)
- Internal shell commands to manage and use BEUIP from within `biceps`

---

---

## Implementation Summary

This project implements a full-featured Unix shell with integrated peer-to-peer networking:

**Shell Core**: Interactive command interpreter with pipes, redirections, and history
**Networking**: Custom BEUIP protocol (broadcast, discovery, private messaging) integrated into shell
**Code Quality**: All functions refactored and maintained under 20-line limit with strict `-Wall -Werror` compilation

Key work completed:
- Refactored monolithic functions into focused helpers
- Each function has single responsibility (e.g., parsing, validation, socket handling)
- Protocol handlers organized by message type
- Build validated with strict compiler flags

### Shell (`biceps` / `gescom`)
- Interactive prompt: `user@machine$` (or `#` for root)
- Persistent readline history (`~/.biceps_history`)
- Internal commands: `exit`, `help`, `cd`, `pwd`, `vers`, `beuip`, `mess`
- External commands via `fork` + `execvp`
- Sequential commands with `;`
- Pipelines (`|`) and redirections (`<`, `<<`, `>`, `>>`, `2>`, `2>>`)
- `Ctrl+C` prompt refresh and clean `Ctrl+D` exit

### BEUIP v1 (`servbeuip` / `clibeuip`)
- UDP server on port `9998`
- Presence broadcast (`code=1`) and ACK (`code=2`)
- Peer table `(ip, pseudo)` with duplicate protection
- `code=3`: list request
- `code=4`: private message (`pseudo\0message` payload)
- `code=5`: message to all
- `code=9`: delivered text
- `code=0`: leave notification
- Security gate: `code 3/4/5` accepted only from `127.0.0.1`
- Graceful shutdown sends `code=0`

### Biceps v2 Internal Network Commands
- `beuip start <pseudo>`: starts `servbeuip` as child process
- `beuip stop`: sends `SIGINT` to child server (graceful stop)
- `exit`: also stops running BEUIP child server automatically
- `mess list`: asks local BEUIP server to print online pseudos
- `mess to <pseudo> <message>`: sends private message
- `mess all <message>`: sends message to all

`vers` now prints versions of:
- `gescom`
- `creme`
- `biceps`

---

## BEUIP Message Format

Message structure:
- Byte 1: code (`'0'`, `'1'`, `'2'`, `'3'`, `'4'`, `'5'`, `'9'`)
- Bytes 2-6: literal tag `BEUIP`
- Bytes 7-end: payload

Payload by code:
- `0`: pseudo leaving
- `1`: pseudo (presence)
- `2`: pseudo (ACK)
- `3`: empty
- `4`: `destPseudo\0message`
- `5`: message text
- `9`: message text

---

## Code Structure

### Module Architecture

**Shell (`biceps` + `gescom`)**:
- `biceps.c` (≈40 lines): Main REPL loop, prompt display, interactive session management
- `gescom.c/h` (≈400 lines): Command parser, execution engine, internal command handlers
  - Parses user input into tokens and redirections
  - Routes to built-in commands (`cd`, `pwd`, `vers`, `beuip`, `mess`) or external executables
  - Manages pipes and I/O redirections (dup2-based redirection)
  - Integrated BEUIP client commands (`mess list`, `mess to`, `mess all`)

**Network Core (`creme`)**:
- `creme.c/h` (≈500 lines): Reusable BEUIP protocol library
  - Message building/parsing functions
  - UDP socket management (broadcast enable, binding)
  - Peer table (IP + pseudo tuple management with duplicate prevention)
  - Server-side datagram dispatch router (delegates to message-type handlers)

**BEUIP Server (`servbeuip`)**:
- `servbeuip.c` (≈14 lines main): UDP server for peer discovery and message broadcast
  - Listens for peer presence announcements and routes messages between peers
  - Maintains active peer list with IP/pseudo tracking
  - Handles message codes: presence (1), ACK (2), list (3), private (4), broadcast (5), leave (0)
  - Security: codes 3/4/5 accepted only from localhost (127.0.0.1)

**BEUIP Client Test (`clibeuip`)**:
- `clibeuip.c` (≈14 lines main): Standalone client for protocol testing
  - Can broadcast presence, query peer list, send private/public messages, signal leave

### Design Decisions

1. **Function Size Constraint**: All functions refactored to < 20 lines for improved:
   - Readability and maintainability
   - Testability and debugging
   - Code reuse through helper composition

2. **Strict Compilation** (`-Wall -Werror`): All warnings treated as errors
   - Catches bugs early
   - Ensures code quality

3. **Modular Helpers**: Large functions decomposed into single-responsibility helpers:
   - Example: Server datagram handling split into type-specific message handlers
   - Command execution split into parsing, validation, and dispatch stages

---

## Build & Compilation

**Requirements:**
- `gcc`
- `libreadline-dev` (for shell history/editing)

**Install readline (Debian/Ubuntu):**
```bash
sudo apt install libreadline-dev
```

**Compiler flags:**
- `-Wall -Werror`: Enable all warnings and treat as errors
- `-g -DTRACE` (debug variant): Enable debug symbols and trace messages

Common targets:
```bash
make beuip        # build servbeuip + clibeuip
make biceps       # build shell (primary deliverable)
make biceps-debug # build with -DTRACE and debug symbols
make biceps-valgrind  # run shell under Valgrind
make clean        # remove all build artifacts
```

---

## Quick Usage

Start shell:
```bash
./biceps
```

Inside `biceps`:
```text
vers
beuip start matheus
mess list
mess to alice hello alice
mess all hello everyone
beuip stop
exit
```

Standalone BEUIP test (without shell):
```bash
# Terminal 1:
./servbeuip matheus

# Terminal 2:
./clibeuip alice           # broadcast presence
./clibeuip 3              # list online peers
./clibeuip 4 alice "hello"  # private message
./clibeuip 5 "hello all"  # message to all
./clibeuip 0 alice        # leave
```

---

## TRACE Mode

Compile with TRACE enabled:
```bash
make biceps-debug
```

This enables conditional debug messages guarded by `#ifdef TRACE`.