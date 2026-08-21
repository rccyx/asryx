#!/usr/bin/env bash
set -Eeuo pipefail

ASRYX_BACKEND="cpu"
# shellcheck disable=SC2034
ASRYX_INSTALL_DEV=0
# shellcheck disable=SC2034
ASRYX_MODEL_NAME="${ASRYX_DEFAULT_MODEL}"

_asryx_parse_build_options() {
  local arg=""

  for arg in "$@"; do
    case "${arg}" in
      --cuda)
        _asryx_select_backend "cuda"
        ;;
      --vulkan)
        _asryx_select_backend "vulkan"
        ;;
      *) _asryx_die "unknown build option: ${arg}" ;;
    esac
  done
}

_asryx_parse_install_options() {
  local arg=""

  for arg in "$@"; do
    case "${arg}" in
      --dev)
        # shellcheck disable=SC2034
        ASRYX_INSTALL_DEV=1
        ;;
      --cuda | --vulkan)
        _asryx_parse_build_options "${arg}"
        ;;
      *) _asryx_die "unknown install option: ${arg}" ;;
    esac
  done
}

_asryx_select_backend() {
  local requested="$1"

  if [[ "${ASRYX_BACKEND}" != "cpu" && "${ASRYX_BACKEND}" != "${requested}" ]]; then
    _asryx_die "CUDA and Vulkan backends cannot be selected together"
  fi

  ASRYX_BACKEND="${requested}"
}
