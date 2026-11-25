#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  printf 'usage: bash tests/rolling-wav.sh ASRYX MONO_16KHZ_PCM16_WAV\n' >&2
  exit 2
fi

runtime_root="$(mktemp -d)"
trap 'rm -rf -- "$runtime_root"' EXIT
session="$runtime_root/asryx"
mkdir "$session"
cp -- "$2" "$session/rec.wav"
touch "$session/stream.finish"

if ! XDG_RUNTIME_DIR="$runtime_root" "$1" transcription-worker; then
  if [[ -f "$session/stream.failed" ]]; then
    cat "$session/stream.failed" >&2
  fi
  exit 1
fi

if [[ ! -f "$session/stream.done" ]]; then
  printf 'rolling worker did not publish a transcript\n' >&2
  exit 1
fi

cat "$session/stream.txt"
printf '\n'
