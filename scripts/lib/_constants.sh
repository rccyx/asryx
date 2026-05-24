#!/usr/bin/env bash

# shellcheck disable=SC2034
ASRYX_DEFAULT_MODEL="base.en"
ASRYX_DEFAULT_LANGUAGE="auto"
ASRYX_CONFIG_FILE=".asryx.conf"
ASRYX_LOCAL_BIN_DIR_REL=".local/bin"
ASRYX_LOCAL_OPT_DIR_REL=".local/opt"
ASRYX_ASRYX_BIN_REL=".local/bin/asryx"
ASRYX_WHISPER_CLI_BIN_REL=".local/bin/whisper-cli"
ASRYX_ASRYX_OPT_DIR_REL=".local/opt/asryx"
ASRYX_WHISPER_DIR_REL=".local/opt/whisper.cpp"
ASRYX_SHARE_DIR_REL=".local/share/asryx"
ASRYX_CACHE_DIR_REL=".cache/asryx"
ASRYX_WHISPER_PIN_REL=".local/share/asryx/versions/whisper-cpp-sha"
ASRYX_RUNTIME_DIR_NAME="asryx"
ASRYX_RUNTIME_TMP_ROOT="/tmp"

_asryx_home_path() {
  printf '%s/%s\n' "${HOME}" "$1"
}

_asryx_config_path() {
  _asryx_home_path "${ASRYX_CONFIG_FILE}"
}

_asryx_runtime_fallback_dir() {
  printf '%s/%s-%s\n' "${ASRYX_RUNTIME_TMP_ROOT}" "${ASRYX_RUNTIME_DIR_NAME}" "$(id -u)"
}
