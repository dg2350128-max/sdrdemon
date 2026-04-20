# Large Exe Builder

Produces a Windows 64-bit console EXE that is at least **120 MB** in size by
embedding a 130 MiB random-data blob into the binary via `objcopy`.

## Prerequisites

| Tool | Ubuntu package |
|------|---------------|
| MinGW-w64 cross-compiler | `mingw-w64` |
| `x86_64-w64-mingw32-objcopy` | included in `mingw-w64` |

```bash
sudo apt-get install mingw-w64
```

## Build

```bash
bash tools/largeexe/build.sh           # output → tools/largeexe/out/
# or specify a custom output directory:
bash tools/largeexe/build.sh /tmp/myout
```

The script prints the final EXE size and exits with code 1 if it is below
120 MiB.

## Expected output

```
[1/4] Generating 130 MiB data blob → …/bigdata.bin
[2/4] Converting blob to PE object   → …/bigdata.o
[3/4] Compiling main.c               → …/main.o
[4/4] Linking                        → …/sdrangel_large.exe
Done. …/sdrangel_large.exe — 136564934 bytes (130.24 MiB)
```

The resulting file is a valid **PE32+ executable (console) x86-64** as
confirmed by `file(1)`.

## Why this approach?

* A plain zero-filled `static char buf[]` in C would end up in the `.bss`
  section and **not** increase the on-disk binary size.
* By converting a raw binary blob with `objcopy --input-target binary` the
  data lands in the `.data` section, which is written verbatim to the EXE.
* MinGW (GCC + GNU binutils for Windows targets) is available as a standard
  Ubuntu package and requires no additional SDK.
