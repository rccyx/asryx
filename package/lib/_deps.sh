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

_asryx_require_cuda_dependencies() {
  if [[ -n "${CUDACXX:-}" ]]; then
    if [[ ! -x "${CUDACXX}" ]]; then
      _asryx_mark_missing "CUDA compiler: ${CUDACXX}"
    fi
    return 0
  fi

  _asryx_require_command nvcc
}

_asryx_require_vulkan_dependencies() {
  if [[ -n "${VULKAN_SDK:-}" && -x "${VULKAN_SDK}/bin/glslc" ]]; then
    return 0
  fi

  _asryx_require_command glslc
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

  printf '\ninstall them with your system package manager and rerun ./package/install\n' >&2
  exit 1
}

_asryx_require_runtime_dependency_tools() {
  _asryx_require_command git
  _asryx_require_command cmake
  _asryx_require_command ninja
  _asryx_require_command install
  _asryx_require_command curl
  _asryx_require_command sha256sum

  _asryx_require_one_command "c compiler" clang gcc cc
  _asryx_require_one_command "c++ compiler" clang++ g++ c++

  _asryx_require_audio_backend
  _asryx_require_clipboard_backend
  _asryx_require_command notify-send
}

_asryx_require_runtime_dependencies() {
  _asryx_require_runtime_dependency_tools
  _asryx_fail_if_missing_tools
}

_asryx_require_backend_dependencies() {
  local backend="$1"

  case "${backend}" in
    cpu) ;;
    cuda) _asryx_require_cuda_dependencies ;;
    vulkan) _asryx_require_vulkan_dependencies ;;
    *) _asryx_mark_missing "unsupported backend: ${backend}" ;;
  esac

  _asryx_fail_if_missing_tools
}

_asryx_require_dep_recipe_value() {
  local name="$1"
  local value="$2"

  [[ -n "${value}" ]] || _asryx_die "dependency recipe is missing ${name}"
}

_asryx_require_git_sha() {
  local dep_name="$1"
  local rev="$2"

  [[ "${rev}" =~ ^[0-9a-f]{40}$ ]] || _asryx_die "invalid ${dep_name} pin: ${rev}"
}

_asryx_dep_installed() {
  local dep_source_dir="$1"
  local dep_rev="$2"
  local dep_ready_path="$3"

  [[ -d "${dep_source_dir}/.git" ]] || return 1
  [[ -e "${dep_ready_path}" ]] || return 1

  local installed_rev=""
  installed_rev="$(git -C "${dep_source_dir}" rev-parse HEAD)"

  [[ "${installed_rev}" == "${dep_rev}" ]]
}

_asryx_sync_git_dep() {
  local dep_name="$1"
  local dep_repo="$2"
  local dep_rev="$3"
  local dep_source_dir="$4"

  _asryx_ensure_dir "$(dirname "${dep_source_dir}")"

  if [[ -d "${dep_source_dir}/.git" ]]; then
    _asryx_log "updating ${dep_name}"
    git -C "${dep_source_dir}" remote set-url origin "${dep_repo}"
  else
    if [[ -e "${dep_source_dir}" ]]; then
      _asryx_die "${dep_source_dir} exists but is not a git checkout"
    fi

    _asryx_log "cloning ${dep_name}"
    git clone --no-checkout "${dep_repo}" "${dep_source_dir}"
  fi

  git -C "${dep_source_dir}" fetch --depth 1 origin "${dep_rev}"
  git -C "${dep_source_dir}" checkout --force --detach "${dep_rev}"
  git -C "${dep_source_dir}" reset --hard "${dep_rev}"
}

_asryx_sync_dep_submodules() {
  local dep_source_dir="$1"

  git -C "${dep_source_dir}" submodule sync --recursive
  git -C "${dep_source_dir}" submodule update --init --recursive
}

_asryx_reset_dep_recipe() {
  unset DEP_NAME DEP_REPO DEP_REV DEP_DIR_REL DEP_BUILD_DIR_REL DEP_INSTALL_DIR_REL
  unset DEP_READY_PATH_REL DEP_SOURCE_DIR DEP_BUILD_DIR DEP_INSTALL_DIR DEP_READY_PATH
  unset DEP_SYNC_SUBMODULES
  unset -f dep_build
}

_asryx_install_dep_recipe() {
  local recipe="$1"

  _asryx_reset_dep_recipe
  # shellcheck source=/dev/null
  source "${recipe}"

  _asryx_require_dep_recipe_value DEP_NAME "${DEP_NAME:-}"
  _asryx_require_dep_recipe_value DEP_REPO "${DEP_REPO:-}"
  _asryx_require_dep_recipe_value DEP_REV "${DEP_REV:-}"
  _asryx_require_dep_recipe_value DEP_DIR_REL "${DEP_DIR_REL:-}"
  _asryx_require_git_sha "${DEP_NAME}" "${DEP_REV}"

  DEP_BUILD_DIR_REL="${DEP_BUILD_DIR_REL:-${DEP_DIR_REL}/build/asryx-release}"
  DEP_INSTALL_DIR_REL="${DEP_INSTALL_DIR_REL:-${DEP_DIR_REL}/install}"
  DEP_READY_PATH_REL="${DEP_READY_PATH_REL:-${DEP_INSTALL_DIR_REL}}"

  DEP_SOURCE_DIR="$(_asryx_home_path "${DEP_DIR_REL}")"
  # shellcheck disable=SC2034
  DEP_BUILD_DIR="$(_asryx_home_path "${DEP_BUILD_DIR_REL}")"
  # shellcheck disable=SC2034
  DEP_INSTALL_DIR="$(_asryx_home_path "${DEP_INSTALL_DIR_REL}")"
  DEP_READY_PATH="$(_asryx_home_path "${DEP_READY_PATH_REL}")"

  _asryx_log "${DEP_NAME} pin: ${DEP_REV}"
  if _asryx_dep_installed "${DEP_SOURCE_DIR}" "${DEP_REV}" "${DEP_READY_PATH}"; then
    _asryx_log "${DEP_NAME} already installed"
    return 0
  fi

  _asryx_sync_git_dep "${DEP_NAME}" "${DEP_REPO}" "${DEP_REV}" "${DEP_SOURCE_DIR}"

  if [[ "${DEP_SYNC_SUBMODULES:-0}" -eq 1 ]]; then
    _asryx_sync_dep_submodules "${DEP_SOURCE_DIR}"
  fi

  if declare -F dep_build >/dev/null; then
    dep_build
  fi
}
