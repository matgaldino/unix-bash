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
**Networking**: Custom BEUIP protocol (broadcast, discovery, messaging, file sharing) integrated into shell
**Code Quality**: All functions maintained under 20-line limit with strict `-Wall -Werror` compilation

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

### Biceps v3 — Multi-threading and File Sharing (TP3)
- **UDP server replaced by a POSIX thread** (`serveur_udp`): shares memory with the shell process, eliminating the need for self-sent UDP messages and closing the man-in-the-middle vulnerability
- **Codes 3/4/5 removed from UDP**: handled directly in-process via `commande()` using the shared peer list — security hardening
- **Mutex-protected peer list**: concurrent access between the UDP thread (writes) and the shell thread (reads) guarded by `pthread_mutex_t`
- **Automatic broadcast address detection**: `getifaddrs()` + `getnameinfo()` enumerate all active interfaces at startup, replacing the hardcoded `192.168.88.255`
- **Peer list replaced by ordered linked list** (`struct elt`): alphabetically sorted by pseudo, dynamically allocated, replaces the fixed-size `creme_peer_table`
- **TCP server thread** (`serveur_tcp`): listens on port `9998`, spawns one detached thread per connection
- **`beuip ls <pseudo>`**: connects via TCP to a peer and retrieves its `reppub/` listing (`ls -l`)
- **`beuip get <pseudo> <nomfic>`**: downloads a file from a peer's `reppub/` over TCP with security checks (no `/`, no `..`, no overwrite of existing local file, remote error detection)
- **Multi-level trace compilation**: `TRACE1` (server lifecycle + protocol events), `TRACE2` (execution flow details); `-DTRACE` enables all levels

---

## BEUIP Message Format

Message structure:
- Byte 1: code (`'0'`, `'1'`, `'2'`, `'9'`)
- Bytes 2-6: literal tag `BEUIP`
- Bytes 7-end: payload

Payload by code:
- `0`: pseudo leaving
- `1`: pseudo (presence broadcast)
- `2`: pseudo (ACK)
- `9`: message text

> Codes `3`, `4`, `5` are no longer transmitted over UDP (biceps v3). They are handled internally via `commande()`.

### TCP File Sharing Protocol (port 9998)
- Client sends `L` → server replies with `ls -l reppub/` output then closes
- Client sends `F<nomfic>\n` → server replies with `cat reppub/<nomfic>` output then closes

---

## Code Structure

### Module Architecture

**Shell (`biceps` + `gescom`)**:
- `biceps.c` (≈40 lines): Main REPL loop, prompt display, interactive session management
- `gescom.c/h` (≈1300 lines): Command parser, execution engine, all internal command handlers
  - Parses user input into tokens and redirections
  - Routes to built-in commands (`cd`, `pwd`, `vers`, `beuip`, `mess`) or external executables
  - Manages pipes and I/O redirections (dup2-based redirection)
  - UDP thread: `serveur_udp`, datagram handlers, presence broadcast via `getifaddrs`
  - TCP thread: `serveur_tcp`, `envoiListe`, `envoiFichier`, per-connection thread dispatch
  - Peer list: `struct elt` linked list with `ajouteElt`, `supprimeElt`, `listeElts`, `findIpByPseudo`
  - Client commands: `demandeListe`, `demandeFichier`, `commande`

**Network Core (`creme`)**:
- `creme.c/h` (≈500 lines): Reusable BEUIP protocol library
  - Message building/parsing functions
  - UDP socket management (broadcast enable, binding)
  - Server-side datagram dispatch router (delegates to message-type handlers)

**BEUIP Server (`servbeuip`)**:
- `servbeuip.c` (≈14 lines main): Standalone UDP server (TP1/TP2 reference implementation)

**BEUIP Client Test (`clibeuip`)**:
- `clibeuip.c` (≈14 lines main): Standalone client for protocol testing

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
beuip start matheus reppub      # starts UDP + TCP servers
mess list                        # list online peers
mess to alice hello alice        # private message
mess all hello everyone          # broadcast message
beuip ls alice                   # list alice's shared files
beuip get alice report.pdf       # download file from alice
beuip stop                       # graceful shutdown
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

Two levels of conditional trace messages, enabled at compile time:

| Flag | Messages included |
|---|---|
| `-DTRACE1` | Server thread lifecycle, protocol code events (`[CODE 1/2]`), security rejections |
| `-DTRACE2` | Execution flow: fork/exec, pipe children, broadcast addresses |
| `-DTRACE` | All levels (master switch, equivalent to `TRACE1` + `TRACE2`) |

```bash
make biceps-debug        # builds with -DTRACE (all levels) + debug symbols
```