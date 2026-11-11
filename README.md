# Veksel

*Named after **Wechsel** (German for "change")*

Live-coded audio effects. Edit C in the window, press Compile, hear the change instantly.

<img src="showcase.png" height="650px"/>

## Using it

Your code goes in the editor. Write a function with the `veksel_main` macro that
takes `input` (the current mic sample) and returns the output sample:
```
veksel_main
{
    return input * 0.5f;
}
```

Press **▶**. If it builds, the new code is swapped into the running stream.
If it doesn't, the errors show in the log and the previous plugin keeps playing.
You get access to `<math.h>`, `<stdlib.h>`, `<stdio.h>` and `M_PI`.

The **⚙** button switches the editor to the audio settings page (output device,
input device, sample rate, buffer size). Press it again to go back to the editor.


## How it works

Three threads, one job each:

- **main**: the ImGui window: editor, ▶ and ⚙ buttons, settings page, log.
- **compiler**: runs clang on its own thread, so the UI stays responsive while it builds.
- **audio**: PortAudio callback, runs the plugin.

The compiler thread builds the `.dylib`/`.so`, loads it, and hands it to the main thread through a mutex-guarded slot. 

The main thread then asks the audio thread to stop using the current plugin, waits for the acknowledgement, swaps in the new one and unloads the old one. 

The audio thread never locks, allocates or calls `dlclose`.
For the moment of the swap, audio passes the input straight through.

## Rules for plugin code

`veksel_main` runs on the audio thread once per sample. Don't allocate, print,
lock, sleep or block. Keep it fast. Static variables work, but reset on every
reload.

## Limits

- Mono in, mono out (for now). The output sample is copied to both speakers.
- A plugin that crashes (segfault) or never returns (infinite loop) takes the whole program down.
- Plugins don't know the sample rate.
- macOS only for now: the compile command uses `-dynamiclib`, and the font path is Menlo.
- Needs clang (path set in the engine settings), PortAudio, GLFW, Dear ImGui and C++20.
- Your code isn't saved. Close the window and it's gone.

## TODO

- [ ] Wire the settings page up to PortAudio (devices, sample rate, buffer size).
- [ ] Survive a broken plugin: run a test in a new process before
      loading, and don't load if it breaks.
- [ ] Save and load the editor text (currently lost on exit).
- [ ] Expose the sample rate to plugins.
- [ ] Stereo plugins (`veksel_main_stereo`).
- [ ] Make it cross platform (`-shared` on Linux, `.dll` on Windows, fonts).
