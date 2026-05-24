#!/usr/bin/env bash
set -Eeuo pipefail

_asryx_log() {
  printf '%s: %s\n' "${ASRYX_LOG_PREFIX:-asryx}" "$*"
}

_asryx_die() {
  printf '%s: error: %s\n' "${ASRYX_LOG_PREFIX:-asryx}" "$*" >&2
  exit 1
}

_asryx_have() {
  command -v "$1" >/dev/null 2>&1
}

_asryx_require_home() {
  [[ -n "${HOME:-}" ]] || _asryx_die "HOME is not set"
  [[ "${HOME}" == /* ]] || _asryx_die "HOME is not absolute: ${HOME}"
  [[ "${HOME}" != "/" ]] || _asryx_die "HOME resolved to /"
}

_asryx_ensure_dir() {
  mkdir -p "$1"
}

_asryx_require_absolute() {
  local path="$1"

  [[ -n "${path}" ]] || _asryx_die "refusing empty path"
  [[ "${path}" == /* ]] || _asryx_die "refusing non-absolute path: ${path}"
  [[ "${path}" != "/" ]] || _asryx_die "refusing /"
  [[ "${path}" != "${HOME}" ]] || _asryx_die "refusing HOME: ${path}"
}

_asryx_remove_one() {
  local path="$1"
  _asryx_require_absolute "${path}"

  if [[ ! -e "${path}" && ! -L "${path}" ]]; then
    return 0
  fi

  _asryx_log "removing ${path}"
  if [[ -d "${path}" && ! -L "${path}" ]]; then
    rm -rf -- "${path}"
  else
    rm -f -- "${path}"
  fi
}