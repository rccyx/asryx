<div align="center">

# asryx

<br/>

<p align="center">
  <a href="https://github.com/rccyx/asryx/actions"><img src="https://img.shields.io/github/actions/workflow/status/rccyx/asryx/ci.yml?style=for-the-badge&color=black&labelColor=111111&logo=githubactions&logoColor=white" alt="CI Status"/></a>
  <a href="#installation"><img src="https://img.shields.io/badge/Platform-Linux-black?style=for-the-badge&color=black&labelColor=111111&logo=linux&logoColor=white" alt="Platform: Linux"/></a>
  <a href="#runtime-model"><img src="https://img.shields.io/badge/Offline-No_Cloud-black?style=for-the-badge&color=black&labelColor=111111" alt="Offline"/></a>
  <a href="https://github.com/rccyx/asryx/blob/main/LICENSE"><img src="https://img.shields.io/badge/License-Apache-black?style=for-the-badge&color=black&labelColor=111111&logo=apache&logoColor=white" alt="License"/></a>
</p>

</div>

<p align="center">
  <a href="./assets/demo.gif">
    <img src="./assets/demo.gif" alt="asryx demo" width="100%">
  </a>
</p>

## Overview

asryx is a native C++ ASR binary for Linux. It builds locally against a pinned `whisper.cpp` source tree, records audio through the active Linux audio stack, runs recognition in-process, writes the transcript to the active clipboard backend, emits desktop notifications, and removes runtime artifacts after completion. Easily installed, and more easily removed.

Uses standard C++ and Linux dependencies, so it works with any Linux machine. Links against `whisper.cpp` as an embedded library through it's public C compatible API. There is no ASR server, hosted API, Python runtime, Node runtime, container layer, resident daemon, GUI process, dashboard, subscription, or network dependency during transcription.

The program is basically a toggle, and a very simple [CLI](#cli).

```bash
asryx
```

The first invocation starts capture.

```bash
asryx
```

The next invocation stops capture, transcribes locally, copies the transcript, notifies the session, and cleans the runtime directory.

The runtime is guarded by an atomic lock directory and live PID checks, so compositor double-fires, key repeat, or repeated invocations collapse into safe no ops while one active phase owns the recorder or transcription. A second process cannot start a parallel recorder, interrupt an active decode, or corrupt the transcript path.

**Runtime model:**

```text
press
  -> acquire lock
  -> start local recorder
  -> write recorder pid
  -> mark state as recording
  -> notify

press again
  -> acquire lock
  -> stop recorder
  -> mark state as transcribing
  -> decode wav into memory
  -> run whisper.cpp inference in-process
  -> trim transcript
  -> write transcript to clipboard
  -> notify
  -> remove runtime files
  -> release lock
```

Audio capture prefers PipeWire:

```text
pw-record
```

ALSA is used as fallback:

```text
arecord
```

Captured audio is written as a temporary WAV file:

```text
mono
16 kHz
signed 16-bit
```

The second invocation stops the recorder by signal, waits for the recorder process to exit, decodes the WAV into memory as float samples, runs local inference, trims the result, and writes it to the clipboard.

Clipboard backends:

```text
wl-copy                     # Wayland
xclip -selection clipboard  # X11 fallback
```

Notifications:

```text
notify-send
```

Whenever the event is emmited, `notify-send` pipes it to Mako, Dunst, or any active desktop notification daemon to render it.

**Runtime state:**

```text
$XDG_RUNTIME_DIR/asryx
```

If `$XDG_RUNTIME_DIR` is unavailable, falls back to:

```text
/tmp/asryx-$UID
```

**Runtime files:**

```text
lock/
rec.pid
rec.wav
rec.err
state
```

After a completed transcription, runtime files are removed. The transcript survives only through the clipboard.

## Installation

```bash
git clone https://github.com/rccyx/asryx
cd asryx && bash ./scripts/install
```

The installer validates the user environment, checks required tools, clones the pinned source, builds the binary locally, installs the executable, writes the version pin, writes the default config, installs the default model, selects it, and prints a PATH note when `~/.local/bin` is unavailable from the current shell.

Installed paths:

```text
~/.local/bin/asryx
~/.local/opt/whisper.cpp
~/.local/share/asryx/
~/.local/share/asryx/versions/whisper-cpp-sha
~/.asryx.conf
```

Default model:

```text
base.en
```

Model downloads pull from [Hugging Face.](https://huggingface.co/ggerganov/whisper.cpp)

After installation:

```bash
asryx status
```

Expected output:

```text
idle
```

`asryx status` prints one of:

```text
idle
recording
transcribing
```

> [!TIP]
> This output can be used for status surfaces such as Waybar and Polybar.

## Dependencies

You probably have most of these already, but check.

Build:

```text
bash
git
curl
cmake
ninja
g++ or clang++
```

Runtime depends on your machine. For audio, check what you have:

```bash
which pw-record || which arecord
```

PipeWire systems have `pw-record`, ALSA systems have `arecord`. If you have neither, install `pipewire` or `alsa-utils` through your package manager.

For clipboard, it depends on your session. Hyprland, Sway, and any other Wayland compositor need `wl-clipboard`. X11 needs `xclip`. If you're not sure which you are on:

```bash
echo "$XDG_SESSION_TYPE"
```

Desktop notifications require an active notification daemon such as Mako, Dunst, or the session's native notification service.

## Keybind

The binary takes no arguments to toggle, so just bind it to a key in whatever compositor or DE you're on.

Hyprland:

```ini
bind = ALT, W, exec, asryx
```

Sway / i3:

```ini
bindsym $mod+w exec asryx
```

GNOME: Settings > Keyboard > Custom Shortcuts, set command to `asryx`.

KDE Plasma: System Settings > Shortcuts > Custom Shortcuts, set command to `asryx`.

> [!TIP]
> A clipboard manager is highly recommended for long recordings. In case you copy something else by mistake after the transcription is emitted.

## CLI

The full surface area:

```text
asryx
asryx status
asryx --language <auto|CODE>
asryx --model list
asryx --model install <MODEL>
asryx --model use <MODEL>
asryx --model uninstall <MODEL>
```

List supported models:

```bash
asryx --model list
```

Install a model:

```bash
asryx --model install small.en
```

Select a model:

```bash
asryx --model use small.en
```

Remove a model:

```bash
asryx --model uninstall small.en
```

Set transcription language:

```bash
asryx --language auto
asryx --language en
asryx --language de
```

## Models

Supported models:

```text
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

| Model              | Disk    | RAM     | Speed vs large |
| ------------------ | ------- | ------- | -------------- |
| tiny / tiny.en     | 75 MiB  | ~273 MB | ~10x           |
| base / base.en     | 142 MiB | ~388 MB | ~7x            |
| small / small.en   | 466 MiB | ~852 MB | ~4x            |
| medium / medium.en | 1.5 GiB | ~2.1 GB | ~2x            |
| large-v3-turbo     | 1.5 GiB | ~2.3 GB | ~8x            |
| large-v1 / v2 / v3 | 2.9 GiB | ~3.9 GB | 1x             |

Speed is relative to large on CPU.

`base.en` is the default. It starts quickly and covers the default English offline transcription path.

Installed models live under:

```text
~/.local/share/asryx/
```

Example:

```text
~/.local/share/asryx/ggml-base.en.bin
```

## Configuration

Configuration is stored in `~/.asryx.conf`.

**Model**

`model` selects the active model. Switching via CLI updates the config instantly:

```bash
asryx --model use small.en
```

**Language**

`language` controls transcription language. `auto` detects the language first, which adds a small amount of latency. Locking to a code skips detection entirely:

```bash
asryx --language es
asryx --language auto
```

English only models (`tiny.en`, `base.en`, `small.en`, `medium.en`) only accept `en` or `auto`. Multilingual models accept any of the 99 supported language codes.

> [!NOTE]
> Language support and transcription quality are a property of the [GGML model weights](https://github.com/ggerganov/whisper.cpp/blob/d682e150908e10caa4c15883c633d7902d685237/src/whisper.cpp#L248).

Invalid model and language values are rejected before recording starts.

Manual edits to the config file are valid:

```text
model=base
language=de
```

<details>
<summary><strong>Supported language codes</strong></summary>

<br/>

| Code | Language       |
| ---- | -------------- |
| en   | english        |
| zh   | chinese        |
| de   | german         |
| es   | spanish        |
| ru   | russian        |
| ko   | korean         |
| fr   | french         |
| ja   | japanese       |
| pt   | portuguese     |
| tr   | turkish        |
| pl   | polish         |
| ca   | catalan        |
| nl   | dutch          |
| ar   | arabic         |
| sv   | swedish        |
| it   | italian        |
| id   | indonesian     |
| hi   | hindi          |
| fi   | finnish        |
| vi   | vietnamese     |
| he   | hebrew         |
| uk   | ukrainian      |
| el   | greek          |
| ms   | malay          |
| cs   | czech          |
| ro   | romanian       |
| da   | danish         |
| hu   | hungarian      |
| ta   | tamil          |
| no   | norwegian      |
| th   | thai           |
| ur   | urdu           |
| hr   | croatian       |
| bg   | bulgarian      |
| lt   | lithuanian     |
| la   | latin          |
| mi   | maori          |
| ml   | malayalam      |
| cy   | welsh          |
| sk   | slovak         |
| te   | telugu         |
| fa   | persian        |
| lv   | latvian        |
| bn   | bengali        |
| sr   | serbian        |
| az   | azerbaijani    |
| sl   | slovenian      |
| kn   | kannada        |
| et   | estonian       |
| mk   | macedonian     |
| br   | breton         |
| eu   | basque         |
| is   | icelandic      |
| hy   | armenian       |
| ne   | nepali         |
| mn   | mongolian      |
| bs   | bosnian        |
| kk   | kazakh         |
| sq   | albanian       |
| sw   | swahili        |
| gl   | galician       |
| mr   | marathi        |
| pa   | punjabi        |
| si   | sinhala        |
| km   | khmer          |
| sn   | shona          |
| yo   | yoruba         |
| so   | somali         |
| af   | afrikaans      |
| oc   | occitan        |
| ka   | georgian       |
| be   | belarusian     |
| tg   | tajik          |
| sd   | sindhi         |
| gu   | gujarati       |
| am   | amharic        |
| yi   | yiddish        |
| lo   | lao            |
| uz   | uzbek          |
| fo   | faroese        |
| ht   | haitian creole |
| ps   | pashto         |
| tk   | turkmen        |
| nn   | nynorsk        |
| mt   | maltese        |
| sa   | sanskrit       |
| lb   | luxembourgish  |
| my   | myanmar        |
| bo   | tibetan        |
| tl   | tagalog        |
| mg   | malagasy       |
| as   | assamese       |
| tt   | tatar          |
| haw  | hawaiian       |
| ln   | lingala        |
| ha   | hausa          |
| ba   | bashkir        |
| jw   | javanese       |
| su   | sundanese      |
| yue  | cantonese      |

</details>

## Uninstallation

```bash
./scripts/uninstall
```

Removes owned files and leaves shared system packages untouched.

Removed paths:

```text
~/.local/bin/asryx
~/.local/opt/whisper.cpp
~/.local/share/asryx
~/.cache/asryx
~/.asryx.conf
$XDG_RUNTIME_DIR/asryx
```

## License

Apache-2.0 © @rccyx
