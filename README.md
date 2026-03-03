# UNIX SHELL - biceps 

**biceps** — *Bel Interpréteur de Commandes des Élèves de Polytech Sorbonne* — is a Unix shell implementation written in C, developed as part of a OS course at Polytech Sorbonne. It implements the core features of a Bourne-style shell using POSIX-standard C libraries.

---

## Features

- **Interactive prompt** — displays `user@machine$` (or `#` for root), built dynamically using `getenv`, `gethostname`, and `getuid`
- **Persistent history** — saved across sessions to `~/.biceps_history` via `readline`, with no consecutive duplicates
- **Sequential commands** — `;` separator: `ls ; pwd ; echo done`
- **Internal commands** — built-ins executed directly in the shell process:
  - `exit` — saves history and exits with a goodbye message
  - `help` — lists all internal commands
  - `cd [dir]` — changes directory (defaults to `$HOME`)
  - `pwd` — prints current working directory
  - `vers` — displays biceps and gescom library versions
- **External commands** — executes any program via `fork` + `execvp` + `waitpid`
- **Pipes** — pipelines of arbitrary length: `ls -l | grep .c | wc -l`
- **I/O Redirections** — `<`, `<<`, `>`, `>>`, `2>`, `2>>`
- **Signal handling** — `Ctrl+C` redisplays the prompt; `Ctrl+D` exits cleanly
- **TRACE mode** — compile with `-DTRACE` for debug output

---

## Project Structure
```
.
├── biceps.c       # Main program: prompt, signal handler, readline loop
├── gescom.c       # Library: command parsing, execution, pipes, redirections, built-ins
├── gescom.h       # Public interface for gescom
├── triceps.c      # Reference minimal shell (provided by course)
└── Makefile
```

Two independently versioned components:
- **biceps** `v1.00` — the shell program
- **gescom** `v1.2` — the command management library

---

## Build

**Requirements:** `gcc`, `libreadline-dev`
```bash
sudo apt install libreadline-dev  # Debian/Ubuntu

make                # build
make biceps-debug   # build with -DTRACE
make biceps-valgrind # run valgrind
make clean
```

---

## Implementation Notes

- All internal functions in `gescom.c` are `static` — only the strictly necessary symbols are exported, verifiable with `nm gescom.o`
- `analyseCom` runs inside each child process in pipelines to avoid race conditions on the global `words` array
- `applyRedirections()` strips redirection tokens from `words[]` inside the child before `execvp`
- `strdup` replaces a hand-written `copyString` from earlier versions (kept commented for reference)