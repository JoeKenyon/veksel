# Veksel

*Named after **Wechsel** (German for "change")*

Live-coded audio effects. Save `plugin.c`, hear the change instantly.

Three threads, one job each:

- **main** - watches `plugin.c`, reads terminal commands.
- **compiler** - runs clang, loads the resulting `.dylib`, frees old plugins.
- **audio** - PortAudio callback, runs the plugin.

No locks on the audio path. No leaks. Hot-reload without stopping the stream.

---

## Requirements

- macOS or Linux
- C++17 compiler (`clang++` or `g++`)
- PortAudio (bundled as a submodule)
- Xcode Command Line Tools (macOS)

## Build

Clone with submodules:

```bash
git clone --recursive https://github.com/joekenyon/veksel.git
cd veksel
```

If you cloned without `--recursive`:

```bash
git submodule update --init --recursive
```

Then build:

```bash
cmake -S . -B build
cmake --build build -j
```

Produces `build/veksel`.

## Run

```bash
./build/veksel
```

You'll see:

```
commands: k1 0.8 (set knob 1) / q (quit)
```

Then write a plugin:

```bash
cat > plugin.c <<'EOF'
veksel_main 
{
    return input * KNOB_1;
}
EOF
```

Then you'll see:

```
plugin reloaded
```

Set a knob:

```
k1 0.3
```

The output gain drops to 30% immediately.

Quit:

```
q
```

---

## Writing a plugin

A plugin is a single function that takes an input sample and returns an output sample.

```c
veksel_main 
{
    return input * KNOB_1;
}
```

For stereo:

```c
veksel_main_stereo 
{
    Stereo out;
    out.left  = input.left  * KNOB_1;
    out.right = input.right * KNOB_2;
    return out;
}
```

### What you get

| Name | Type | Range |
|---|---|---|
| `input` | float (mono) / Stereo (stereo) | -1.0 to 1.0 |
| `KNOB_1` .. `KNOB_12` | float | 0.0 to 1.0 |
| `SWITCH_1` .. `SWITCH_3` | int | 0 or 1 |
| `SAMPLE_RATE` | double | 44100, 48000, etc. |
| `M_PI` | constant | 3.14159... |

You can use `<math.h>` functions (`sinf`, `cosf`, `fabsf`, `tanhf`, `powf`, `expf`, `floorf`, `fmodf`, `sqrtf`), and you can declare global variables (they're your plugin's state).

### Rules

- **No `malloc` / `free`.** The audio callback runs in real time - allocating can glitch or crash.
- **No `printf` / file I/O / locks.** Same reason.
- **No sleeping.** Same reason.
- **Statics reset on save.** When you save a new version, the old plugin is unloaded and its statics are freed. The new plugin starts with fresh state.

If you break these rules, the host crashes. `Ctrl-C`, fix the plugin, restart.

### Plugin lifecycle

1. You save `plugin.c`.
2. The file watcher (main thread) sees the change.
3. The compiler thread runs `clang -O1 -shared -fPIC` on the source.
4. If it compiles, `dlopen` loads it.
5. The audio thread swaps it in at the next callback boundary.
6. The old plugin is `dlclose`'d.
7. Statics in the new plugin start fresh.

### Example

**Delay**

```c
#define MAX_DELAY 96000

float buffer[MAX_DELAY];
int write_ptr = 0;

veksel_main
{
    int max = (int)SAMPLE_RATE;
    if (max > MAX_DELAY - 1) max = MAX_DELAY - 1;

    int d = (int)(KNOB_1 * max) + 1;
    float fb = KNOB_2 * 0.9f;

    int rp = write_ptr - d;
    if (rp < 0) rp += MAX_DELAY;

    float delayed = buffer[rp];
    buffer[write_ptr] = input + delayed * fb;

    write_ptr++;
    if (write_ptr >= MAX_DELAY) write_ptr = 0;

    return input * (1.0f - KNOB_3) + delayed * KNOB_3;
}
```

---

## TODO

1. add GUI using imGui

## Licence

Do whatever you want with it.
