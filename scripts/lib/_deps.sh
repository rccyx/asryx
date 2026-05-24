#!/usr/bin/env bash
set -Eeuo pipefail

_asryx_missing_tools=()

_asryx_mark_missing() {
  _asryx_missing_tools+=("$1")
}

_asryx_require_command() {
  local command_name="$1"

  if ! _asryx_have "${command_name}"; then
    _asryx_mark_missing "${command_name}"
  fi
}

_asryx_require_one_command() {
  local label="$1"
  shift

  local command_name=""
  for command_name in "$@"; do
    if _asryx_have "${command_name}"; then
      return 0
    fi
  done

  _asryx_mark_missing "${label}: $*"
}

_asryx_require_audio_backend() {
  if [[ -n "${XDG_RUNTIME_DIR:-}" && -S "${XDG_RUNTIME_DIR}/pipewire-0" ]]; then
    _asryx_require_command pw-record
    return 0
  fi

  _asryx_require_one_command "audio recorder" pw-record arecord
}

_asryx_require_clipboard_backend() {
  if [[ -n "${WAYLAND_DISPLAY:-}" ]]; then
    _asryx_require_command wl-copy
    return 0
  fi

  if [[ -n "${DISPLAY:-}" ]]; then
    _asryx_require_command xclip
    return 0
  fi

  _asryx_require_one_command "clipboard backend" wl-copy xclip
}

_asryx_fail_if_missing_tools() {
  local tool=""

  if [[ "${#_asryx_missing_tools[@]}" -eq 0 ]]; then
    return 0
  fi

  printf '%s: error: missing required tools:\n' "${ASRYX_LOG_PREFIX:-asryx}" >&2

  for tool in "${_asryx_missing_tools[@]}"; do
    printf '  - %s\n' "${tool}" >&2
  done

  printf '\ninstall them with your system package manager and rerun ./scripts/install\n' >&2
  exit 1
}

_asryx_require_runtime_dependencies() {
  _asryx_require_command git
  _asryx_require_command cmake
  _asryx_require_command ninja
  _asryx_require_command install
  _asryx_require_command curl

  _asryx_require_one_command "c compiler" cc gcc clang
  _asryx_require_one_command "c++ compiler" c++ g++ clang++

  _asryx_require_audio_backend
  _asryx_require_clipboard_backend
  _asryx_require_command notify-send

  _asryx_fail_if_missing_tools
}
