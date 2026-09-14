# Performance Analyzer V3

## Build (Windows + MinGW)

Open PowerShell or Command Prompt in this folder and run:

```powershell
mingw32-make clean
mingw32-make -B
```

## Graphical Web Dashboard (Default)

Running `analyzer.exe` with no arguments (or double-clicking it) launches the embedded dashboard server and automatically opens your default web browser:

```powershell
.\analyzer.exe
```

Or explicitly:

```powershell
.\analyzer.exe --gui
.\analyzer.exe --gui --port 8080
.\analyzer.exe --gui --no-browser
```

## Terminal Benchmark CLI

```powershell
.\analyzer.exe --help
.\analyzer.exe --quick
.\analyzer.exe --quick --sizes 500,1000
.\analyzer.exe --quicksort --sizes 100,500,1000
.\analyzer.exe --all --iterations 20 --warmup 5
```

`--quick` is a fast smoke-test mode over the full benchmark suite. Its defaults are 100 and 1000 elements, 10 measurement runs, 2 warmups, and Random input. Explicit `--sizes`, `--iterations`, and `--warmup` values override those quick-mode defaults.

Quick Sort itself is selected with `--quicksort` to avoid a CLI name collision.
