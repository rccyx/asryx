#!/usr/bin/env bash
set -Eeuo pipefail

_asryx_install_default_assets() {
  _asryx_write_default_config_if_missing
  _asryx_install_vad_model
  _asryx_install_and_select_model
  _asryx_warn_path_if_needed
}

_asryx_write_default_config_if_missing() {
  local cfg=""
  cfg="$(_asryx_config_path)"
  [[ -e "${cfg}" ]] && return 0

  _asryx_log "writing ${cfg}"
  printf 'model=%s\nlanguage=%s\npipe_to=\n' "${ASRYX_MODEL_NAME}" "${ASRYX_DEFAULT_LANGUAGE}" >"${cfg}"
}

_asryx_install_vad_model() {
  local target=""
  target="$(_asryx_home_path "${ASRYX_MODELS_DIR_REL}")/${ASRYX_VAD_MODEL_FILE}"
  local tmp="${target}.tmp.$$"

  if [[ -s "${target}" ]]; then
    _asryx_log "VAD model already installed: ${target}"
    return 0
  fi

  _asryx_ensure_dir "$(_asryx_home_path "${ASRYX_MODELS_DIR_REL}")"
  _asryx_log "downloading VAD model ${ASRYX_VAD_MODEL_NAME}"

  if ! curl -fL --retry 3 -o "${tmp}" "${ASRYX_VAD_MODEL_URL}"; then
    rm -f -- "${tmp}"
    _asryx_die "failed to download VAD model"
  fi

  [[ -s "${tmp}" ]] || _asryx_die "downloaded VAD model is empty"
  mv -f "${tmp}" "${target}"
}

_asryx_install_and_select_model() {
  local asryx_bin=""
  asryx_bin="$(_asryx_home_path "${ASRYX_ASRYX_BIN_REL}")"

  [[ -x "${asryx_bin}" ]] || _asryx_die "missing installed binary: ${asryx_bin}"
  "${asryx_bin}" --model install "${ASRYX_MODEL_NAME}"
  "${asryx_bin}" --model use "${ASRYX_MODEL_NAME}"
}

_asryx_warn_path_if_needed() {
  local bin_dir=""
  bin_dir="$(_asryx_home_path "${ASRYX_LOCAL_BIN_DIR_REL}")"

  case ":${PATH}:" in
    *":${bin_dir}:"*) return 0 ;;
  esac

  _asryx_log "note: ${bin_dir} is not in PATH"
  _asryx_log "      add: export PATH=\"${bin_dir}:\$PATH\""
}
