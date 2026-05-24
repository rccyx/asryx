<div align="center">

# A S R Y X

<br/>

<p align="center">
  <a href="https://github.com/rccyx/asryx/actions"><img src="https://img.shields.io/github/actions/workflow/status/rccyx/asryx/ci.yml?style=for-the-badge&color=black&labelColor=111111&logo=githubactions&logoColor=white" alt="CI Status"/></a>
  <a href="#installation"><img src="https://img.shields.io/badge/Platform-Linux-black?style=for-the-badge&color=black&labelColor=111111&logo=linux&logoColor=white" alt="Platform: Linux"/></a>
  <a href="#installation"><img src="https://img.shields.io/badge/Offline-No_Cloud-black?style=for-the-badge&color=black&labelColor=111111" alt="Offline"/></a>
  <a href="https://github.com/rccyx/asryx/blob/main/LICENSE"><img src="https://img.shields.io/github/license/rccyx/asryx?style=for-the-badge&color=black&labelColor=111111&logo=apache&logoColor=white" alt="License"/></a>
</p>

</div>

## Overview

<p align="center">
  <a href="https://www.youtube.com/watch?v=lVDFpoCkvh8">
    <img src="./assets/demo.gif" alt="asryx demo" width="100%">
  </a>
</p>

<details>
<summary><strong>Why?</strong></summary>

<br/>

Voice is always faster than typing, it's supposed to be instant.

The problem is everything built around the tooling: hold a key (pessimal), open an app (bloated), pick a provider/model (decision fatigue), send audio to a server (privacy), wait for a response (speed), hope the network holds (unreliable).

Basically dependency hell.

</details>

<details>
<summary><strong>What?</strong></summary>

<br/>

A lightweight native Linux voice-to-text toggle. Does one thing, and does it well. You compile it on your own machine.

The way it works is:

Press once to record. Talk as long as you want. Press again to stop, it transcribes locally, copies the text to your clipboard, notifies you, and cleans up.

```bash
asryx
```

It pushes a notification saying "recording..."

You speak.

```bash
asryx
```

Says "copied to clipboard"

You paste the text anywhere.

That's it.

Repeated key presses are safe. If a compositor double-fires the keybind or the key repeats, it **won't** spawn five recorders or corrupt the active transcription.

Also, the temporary audio is cleaned after the run. The transcript is copied from memory into the clipboard.

You might also want to keybind it, for example, on Hyprland:

```ini
bind = ALT, W, exec, asryx
```

Also, make sure you **set** a clipboard manager, so the transcript is recoverable if you accidentally copy something else after a long recording.

</details>

<details>
<summary><strong>How is this different?</strong></summary>
<br/>

Basically:

No cloud. No GUI(s). No Python env hell. No background daemon(s). No dashboard(s). No container(s). No subscription(s), no persistent key pressing, no choose from these 965 models you'll never use, no do these 22 steps first and maybe it works. No Node, no Python again.

</details>

<details>
<summary><strong>How it works?</strong></summary>

<br/>

The binary embeds `whisper.cpp` inside a native Linux runtime.

Asryx owns everything around it: recording lifecycle, toggle state, locking, model lookup, local inference, clipboard, notifications, cleanup.

The installer clones the pinned [whisper.cpp](https://github.com/ggml-org/whisper.cpp) source into `~/.local/opt/whisper.cpp`, then builds `asryx` against it on your machine. Transcription happens inside the `asryx` process through the `whisper.cpp` library API.

```text
press
  -> start local recorder
  -> write recorder pid
  -> mark state as recording

press again
  -> stop recorder
  -> read the wav
  -> run whisper.cpp in-process
  -> copy transcript to clipboard
  -> notify
  -> clean runtime files
```

The first press starts the recorder. PipeWire through `pw-record` is preferred, falls back to ALSA through `arecord`. Audio is captured as a temporary WAV file: mono, 16 kHz, signed 16-bit.

The second press stops the recorder by signal, waits for the WAV to finalize, decodes it into memory, runs local inference through `whisper.cpp`, trims the transcript, and pushes it into the clipboard.

Clipboard output:

```text
wl-copy                     # Wayland
xclip -selection clipboard  # X11 fallback
```

Notifications:

```text
notify-send
```

The binary emits the event. Mako, Dunst, or your desktop environment displays it.

Runtime state lives under:

```text
$XDG_RUNTIME_DIR/asryx
```

Falls back to `/tmp/asryx-$UID` if `$XDG_RUNTIME_DIR` is unavailable.

The directory holds only temporary payloads:

```text
lock/
rec.pid
rec.wav
rec.err
state
```

After a successful transcription, all of it is deleted. The transcript survives only through the clipboard.

</details>

## Installation

```bash
git clone https://github.com/rccyx/asryx
cd asryx && bash ./scripts/install
```

The install script does not install system packages. It checks that the required tools exist and exits with a missing-tool list if the machine is not ready.

Ensure these utilities are installed before running the script:

Core Utilities:

- bash
- git
- curl
- install

Build Tools:

- cmake
- ninja
- a C++ compiler (`g++` or `clang++`)

Audio Capture:

- `pw-record` (PipeWire) or `arecord` (ALSA fallback)

Clipboard & Alerts:

- `wl-copy` (Wayland) or `xclip` (X11 fallback).
- `notify-send`

> [!IMPORTANT]
> To receive visual desktop alerts such as `recording...` or `copied...`, ensure a notification daemon like Mako or Dunst is active.

<details>
<summary><strong>What the installer does</strong></summary>

<br/>

It runs in this order.

- validates your home directory
- checks required tools
- clones or updates the pinned `whisper.cpp` source into `~/.local/opt/whisper.cpp`
- builds the native binary locally against `whisper.cpp`
- installs `asryx` into `~/.local/bin/asryx`
- writes the installed `whisper.cpp` pin
- writes the default config if missing
- installs the default model
- selects the default model
- prints a PATH note if `~/.local/bin` is not already available

The default model is:

```text
base.en
```

Models are downloaded through the official `whisper.cpp` model downloader (which is HuggingFace).

The install uses:

```text
~/.local/bin/asryx
~/.local/opt/whisper.cpp
~/.local/share/asryx
~/.local/share/asryx/versions/whisper-cpp-sha
~/.asryx.conf
```

</details>

After installation. You should see the current runtime state.

```text
$ asryx status
idle
```

You can use this if you ever need to know what the engine is currently doing (useful for UI scripts like Polybar or Waybar).

Outputs: (idle, recording, or transcribing)

## Uninstallation

```bash
./scripts/uninstall
```

<details>
<summary><strong>What the uninstaller does</strong></summary>

<br/>

It removes asryx owned files.

```text
~/.local/bin/asryx
~/.local/opt/asryx
~/.local/opt/whisper.cpp
~/.local/share/asryx
~/.cache/asryx
~/.asryx.conf
$XDG_RUNTIME_DIR/asryx
```

It doesn't remove shared system packages.

It refuses dangerous paths such as `/`, `$HOME`, empty paths, and non-absolute paths.

</details>

## CLI

Surface area:

```
asryx
asryx status
asryx --model list
asryx --model install <MODEL>
asryx --model use <MODEL>
asryx --model uninstall <MODEL>
```

## Models

List models.

```bash
asryx --model list
```

Install a model.

```bash
asryx --model install small.en
```

Use a model.

```bash
asryx --model use small.en
```

Remove a model.

```bash
asryx --model uninstall small.en
```

Supported models:

```
tiny.en
tiny
base.en
base
small.en
small
medium.en
medium
large-v1
large-v2
large-v3
large-v3-turbo
large
```

Models ending in `.en` are English-only. All others are multilingual.

| Model              | Disk    | RAM     | Speed vs large |
| ------------------ | ------- | ------- | -------------- |
| tiny / tiny.en     | 75 MiB  | ~273 MB | ~10x           |
| base / base.en     | 142 MiB | ~388 MB | ~7x            |
| small / small.en   | 466 MiB | ~852 MB | ~4x            |
| medium / medium.en | 1.5 GiB | ~2.1 GB | ~2x            |
| large-v3-turbo     | 1.5 GiB | ~2.3 GB | ~8x            |
| large-v1 / v2 / v3 | 2.9 GiB | ~3.9 GB | 1x             |

Speed is relative to large on CPU.

`base.en` is the default. It covers most use cases and starts fast.

Installed models live in:

```bash
~/.local/share/asryx/
```

For example:

```bash
~/.local/share/asryx/ggml-base.en.bin
```

The active model config is stored in:

```bash
~/.asryx.conf
```

## Config

Switching models through the CLI updates this file.

```bash
asryx --model use small.en
```

You can also edit it manually.

```bash
model=small.en
```

## Audio

Prefers PipeWire.

```text
pw-record
```

If unavailable, it falls back to ALSA.

```text
arecord
```

## License

Apache-2.0 © @rccyx
