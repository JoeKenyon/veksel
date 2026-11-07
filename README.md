# Veksel

*Named after **Wechsel** (German for "change")*

Live-coded audio effects. Save `plugin.c`, hear the change instantly.

Three threads, one job each:

- **main** - watches `plugin.c`, reads terminal commands.
- **compiler** - runs clang, loads the resulting `.dylib`, frees old plugins.
- **audio** - PortAudio callback, runs the plugin.

No locks on the audio path. No leaks. Hot-reload without stopping the stream.
