# asryx

<p align="center">
  <a href="./assets/demo.gif">
    <img src="./assets/demo.gif" alt="asryx demo" width="100%">
  </a>
</p>

## Overview

This is a native C++ ASR binary toggle/CLI for Linux.

Runs local transcription against [GGML Whisper](https://github.com/ggml-org/whisper.cpp) models through a [pinned](./versions/) native inference backend, linked in process through its public C compatible API.

The model supplies inference and all 99 language support options, while `asryx` owns the entire native Linux runtime around it.

Records audio through the active Linux audio stack, runs recognition in process with local VAD, writes the transcript to the active clipboard backend, optionally pipes it to a user command, emits desktop notifications, and removes runtime artifacts after completion.

Easily [installed](#installation), and more easily [removed](#uninstallation).

Doesn't stay in memory between uses. 0MB idle RAM. Doesn't load the model unless invoked.

Boots instantly and exits instantly.

One command install. You compile the program on your own machine, no package manager or third parties.

It uses standard C++ and Linux [dependencies](#dependencies), and it's CPU only by default, so it works with any machine, regardless of distro or GPU model.

GGML models are [downloaded locally](#models) and transcription runs against those weights on your machine.

[Reliable](#mechanism) is the word. Repeated invocations, key repeat, stale locks, interrupted sessions, and abandoned runtime artifacts are handled before a new recording begins.

There is no ASR server, background daemon, hosted API, Python runtime, Node runtime, container layer, resident daemon, GUI process, dashboard, subscription, or network dependency during transcription.

## Usage

The program is a toggle.

```bash
asryx
```

The first invocation starts capture.

You talk for as long as you want.

```bash
asryx
```

The next invocation stops capture, transcribes locally, copies the transcript, notifies the session, and cleans the runtime directory.

> [!TIP]
> This toggle can be hooked to Sway, Hyprland, i3, GNOME, KDE, etc.

And a very simple [CLI](#cli):

```text
asryx                         # Toggle record/transcribe
asryx status                  # Check idle/recording/transcribing
asryx --pipe-to '<COMMAND>'   # Set post copy pipe command
asryx --no-pipe               # Clear post copy pipe command
asryx --language <auto|CODE>  # Set language
asryx --model list            # List supported models
asryx --model install <MODEL> # Download model
asryx --model use <MODEL>     # Switch model
```

## Mechanism

My biggest problem with this ecosystem is relying on black box tools without knowing what they do to my system. So, here's exactly how this tool works:

Audio capture orchestration, runtime state, model management, clipboard delivery, notification dispatch, cleanup, and failure recovery are all owned by the binary.

The audio path is native code. Captured audio is validated as RIFF/WAVE, walked chunk by chunk, checked for 16 kHz mono signed 16-bit PCM, and decoded from PCM16 into float samples before inference.

Inference runs in process against the selected GGML Whisper model. A local Silero VAD model is passed into the same inference request so non speech regions are suppressed during recognition instead of leaking silence, dead air, breath noise, keyboard noise, chair noise, or empty regions into the transcript.

Dependency free as in, not even a WAV library sits between the recorder output and the model input.

If your Linux system already plays audio, shows notifications, has a clipboard, and can compile a C++ project, you have everything you need.

The recorder is launched as a native Linux process, tracked through its PID, stopped by signal, and verified to have exited before transcription begins.

An atomic lock directory guards the runtime. Directory creation is the mutex primitive, and ownership is tracked through live PIDs.

Stale sessions are recovered automatically, and repeated invocations coalesce into a single active operation, so key repeats, compositor double fires, interrupted sessions, and abandoned runtime artifacts resolve safely before a new recording begins.

**Runtime model:**

```text
press
  -> acquire lock
  -> load config
  -> validate selected model
  -> validate VAD model
  -> start local recorder
  -> write recorder pid
  -> mark state as recording
  -> notify

talk...

press again
  -> acquire lock
  -> stop recorder
  -> mark state as transcribing
  -> parse RIFF/WAVE
  -> validate format chunk
  -> locate data chunk
  -> decode PCM16 into float samples
  -> run local inference in-process
  -> trim transcript
  -> write transcript to clipboard
  -> if pipe_to is configured, pipe transcript to command stdin
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

Clipboard backends:

```text
wl-copy                     # Wayland
xclip -selection clipboard  # X11 fallback
```

Notifications:

```text
notify-send
```

Whenever the event is emitted, `notify-send` pipes it to Mako, Dunst, or any active desktop notification daemon to render it.

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
error.log
state
```

After a completed transcription, runtime files are completely removed. The transcript is always copied to the clipboard first. If `pipe_to` is configured, the same transcript is then piped into that command's stdin.

> [!NOTE]
> The clipboard is the permanent backup path. If the custom command exits non zero or otherwise fails at the process level, the transcript remains in the clipboard so the text isn't lost.

## Installation

```bash
git clone https://github.com/rccyx/asryx
cd asryx
bash ./package/install
```

> [!TIP]
> If you've changed your mind, see the [uninstaller](#uninstallation).

### CPU

The command above works on any machine and it defaults to the CPU.

If you have a potato it will work on it.

### GPU

Now, the GPU is actually way faster so you might want to use it. See [GPU builds](./docs/gpu.md).

> [!NOTE]
> `main` is branch protected and force pushes are disabled. Anything merged into `main` builds and installs. if you want a versioned snapshot instead, see [releases](https://github.com/rccyx/asryx/releases).

### What it does

The installer validates the user environment, checks required tools, clones the pinned native inference source, builds the binary locally, installs the executable, writes the version pin, writes the default config, installs the VAD model, installs the default transcription model, selects it, and prints a PATH note when `~/.local/bin` is unavailable from the current shell.

Installed paths:

```text
~/.local/bin/asryx
~/.local/share/asryx/
~/.local/share/asryx/deps/whisper.cpp
~/.local/share/asryx/models/ggml-silero-v6.2.0.bin
~/.local/share/asryx/versions/whisper-cpp-sha
~/.asryx.conf
```

Default transcription model:

```text
base.en
```

Default VAD model:

```text
ggml-silero-v6.2.0.bin
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

prints one of:

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

The default CPU build needs no CUDA or Vulkan packages. GPU builds need extra system packages, see [GPU builds](./docs/gpu.md).

Runtime depends on your machine. For audio, check what you have:

```bash
which pw-record || which arecord
```

PipeWire systems have `pw-record`, ALSA systems have `arecord`. If you have neither, install `pipewire` or `alsa-utils` through your package manager.

For clipboard, it depends on your session. Hyprland, Sway, and any other Wayland compositor need `wl-clipboard`. X11 needs `xclip`. If you're not sure which you're on:

```bash
echo "$XDG_SESSION_TYPE"
```

> [!IMPORTANT]
> Desktop notifications require an active notification daemon such as Mako, Dunst, or the session's native notification service.

## Keybind

The binary takes no arguments to toggle, so bind it to a key in the active compositor or desktop environment.

I personally use `Alt + W`, so the config for each DE/WM becomes:

Hyprland:

```ini
bind = ALT, W, exec, asryx
```

Sway / i3:

```ini
bindsym $mod+w exec asryx
```

GNOME:

```text
Settings > Keyboard > Custom Shortcuts
command: asryx
```

KDE Plasma:

```text
System Settings > Shortcuts > Custom Shortcuts
command: asryx
```

> [!WARNING]
> A clipboard manager is highly recommended for long recordings. In case you copy something else by mistake after the transcription is emitted.

## CLI

The full surface area:

```text
asryx
asryx status
asryx --pipe-to '<COMMAND>'
asryx --no-pipe
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

Set the post copy pipe hook:

```bash
asryx --pipe-to 'tee -a ~/transcripts.txt'
```

`asryx --pipe-to '<COMMAND>'` simply updates `pipe_to` in `~/.asryx.conf` and exits. You can also do it manually.

`--pipe-to` is just a shell command string, so it can be a script path, a binary, or any command expression the shell can run.

> [!IMPORTANT]
> It doesn't verify that the command exists, resolves on `PATH`, or is executable. The command string can point at a shell script, binary, or any command expression. That's your responsibility to verify.

Clear the post copy pipe hook:

```bash
asryx --no-pipe
```

This simply clears `pipe_to` in `~/.asryx.conf` and exits.

Clipboard output is the default and always happens first. A completed clipboard only transcription sends this notification:

```text
transcription copied to clipboard.
```

`pipe_to` configures an optional post copy hook:

```text
pipe_to=tee -a /tmp/transcripts.txt
```

When `pipe_to` is non empty, bare `asryx` copies the transcript to the system clipboard first, then pipes the same text into `pipe_to` through stdin. The pipe path sends this notification:

```text
piped and copied to clipboard.
```

## Models

Supported transcription models:

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

`ggml-silero-v6.2.0.bin` is installed alongside the transcription models and used automatically for voice activity detection.

Installed models live under:

```text
~/.local/share/asryx/
~/.local/share/asryx/models/
```

Examples:

```text
~/.local/share/asryx/models/ggml-base.en.bin
~/.local/share/asryx/models/ggml-silero-v6.2.0.bin
```

## Configuration

Configuration is stored in `~/.asryx.conf`.

**Model**

`model` selects the active transcription model. Switching via CLI updates the config instantly:

```bash
asryx --model use small.en
```

**Language**

`language` controls transcription language. `auto` detects the language first, which adds a small amount of unnecessary latency if you speak the same language all the time. Locking to a code skips detection entirely.

```bash
asryx --language es
asryx --language auto
```

English only models (`tiny.en`, `base.en`, `small.en`, `medium.en`) only accept `en` or `auto`. Multilingual models accept any of the 99 supported language codes.

> [!NOTE]
> Language support and transcription quality are a property of the [GGML model weights](https://github.com/ggerganov/whisper.cpp/blob/d682e150908e10caa4c15883c633d7902d685237/src/whisper.cpp#L248).

Invalid model and language values are rejected before recording starts.

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
./package/uninstall
```

Simply removes the owned files:

```text
~/.local/bin/asryx
~/.local/share/asryx
~/.asryx.conf
$XDG_RUNTIME_DIR/asryx
```

> [!NOTE]
> Deletion goes through owned [path validation](/src/platform/fs.cpp) before files or directories are even removed.

## License

Apache-2.0 © @rccyx
