# UNIX SHELL - biceps

`biceps` is a Unix shell written in C for an Operating Systems course.
It now includes:
- Shell core (`biceps` + `gescom`)
- Network module (`creme`)
- BEUIP protocol server/client (`servbeuip`, `clibeuip`)
- Internal shell commands to control and use BEUIP from inside `biceps`

---

## Implemented Features

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

## Project Files

```text
.
├── biceps.c       # shell main loop and prompt
├── gescom.c       # command engine and internal commands
├── gescom.h
├── creme.c        # reusable BEUIP/network logic
├── creme.h
├── servbeuip.c    # BEUIP server executable
├── clibeuip.c     # BEUIP test client executable
├── servudp.c      # basic UDP server example
├── cliudp.c       # basic UDP client example
├── triceps.c      # minimal reference shell
└── Makefile
```

Versioned components used by `biceps`:
- `biceps` shell: `v1.0`
- `gescom` module: `v1.2`
- `creme` module: `v1.0`

---

## Build

Requirements:
- `gcc`
- `libreadline-dev`

Install readline (Debian/Ubuntu):
```bash
sudo apt install libreadline-dev
```

Common targets:
```bash
make udp          # build servudp + cliudp
make beuip        # build servbeuip + clibeuip
make biceps       # build shell
make biceps-debug # build with -DTRACE and debug symbols
make biceps-valgrind
make clean
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

Standalone BEUIP test:
```bash
./servbeuip matheus
./clibeuip alice
./clibeuip 3
./clibeuip 4 alice "hello"
./clibeuip 5 "hello all"
./clibeuip 0 alice
```

---

## TRACE Mode

Compile with TRACE enabled:
```bash
make biceps-debug
```

This enables conditional debug messages guarded by `#ifdef TRACE`.