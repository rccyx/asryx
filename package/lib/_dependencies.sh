#!/usr/bin/env bash
set -Eeuo pipefail

_asryx_install_package_deps() {
  local recipes=("${ASRYX_PACKAGE_DIR}/lib/deps/whisper-cpp.sh")

  if [[ "${ASRYX_INSTALL_DEV}" -eq 1 ]]; then
    recipes+=("${ASRYX_PACKAGE_DIR}/lib/deps/libassert.sh")
  fi

  local recipe=""
  for recipe in "${recipes[@]}"; do
    _asryx_install_dep_recipe "${recipe}"
  done
}

_asryx_install_dep_recipe() {
  local recipe="$1"

  _asryx_reset_dep_recipe
  # shellcheck source=/dev/null
  source "${recipe}"
  _asryx_prepare_dep_recipe

  # shellcheck disable=SC2153
  local dep_name="${DEP_NAME}"
  # shellcheck disable=SC2153
  local dep_repo="${DEP_REPO}"
  # shellcheck disable=SC2153
  local dep_rev="${DEP_REV}"
  local dep_source_dir="${DEP_SOURCE_DIR}"

  _asryx_log "${dep_name} pin: ${dep_rev}"
  if _asryx_dep_installed "${dep_source_dir}" "${dep_rev}" "${DEP_READY_PATH}"; then
    _asryx_log "${dep_name} already installed"
    return 0
  fi

  _asryx_sync_git_dep "${dep_name}" "${dep_repo}" "${dep_rev}" "${dep_source_dir}"
  _asryx_sync_dep_submodules_if_needed
  _asryx_build_dep_if_needed
}

_asryx_prepare_dep_recipe() {
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

_asryx_sync_dep_submodules_if_needed() {
  if [[ "${DEP_SYNC_SUBMODULES:-0}" -ne 1 ]]; then
    return 0
  fi

  git -C "${DEP_SOURCE_DIR}" submodule sync --recursive
  git -C "${DEP_SOURCE_DIR}" submodule update --init --recursive
}

_asryx_build_dep_if_needed() {
  if declare -F dep_build >/dev/null; then
    dep_build
  fi
}

_asryx_reset_dep_recipe() {
  unset DEP_NAME DEP_REPO DEP_REV DEP_DIR_REL DEP_BUILD_DIR_REL DEP_INSTALL_DIR_REL
  unset DEP_READY_PATH_REL DEP_SOURCE_DIR DEP_BUILD_DIR DEP_INSTALL_DIR DEP_READY_PATH
  unset DEP_SYNC_SUBMODULES
  unset -f dep_build
}
