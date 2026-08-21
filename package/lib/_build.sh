#!/usr/bin/env bash
set -Eeuo pipefail

ASRYX_COMPILER_CC=""
ASRYX_COMPILER_CXX=""

_asryx_build_preset() {
  case "${ASRYX_BACKEND}" in
    cpu)
      printf '%s\n' "release"
      ;;
    cuda)
      printf '%s\n' "release-cuda"
      ;;
    vulkan)
      printf '%s\n' "release-vulkan"
      ;;
    *)
      _asryx_die "internal error: unsupported backend ${ASRYX_BACKEND}"
      ;;
  esac
}

_asryx_build_package() {
  local preset=""
  preset="$(_asryx_build_preset)"

  _asryx_select_compiler
  _asryx_select_generator
  _asryx_configure_asryx "${preset}"
  _asryx_compile_asryx "${preset}"
}

_asryx_install_asryx_binary() {
  local preset="$1"
  local src_bin="${ASRYX_REPO_DIR}/build/${preset}/asryx"
  local dst_bin=""
  dst_bin="$(_asryx_home_path "${ASRYX_ASRYX_BIN_REL}")"

  [[ -x "${src_bin}" ]] || _asryx_die "missing built binary: ${src_bin}"
  _asryx_ensure_dir "$(_asryx_home_path "${ASRYX_LOCAL_BIN_DIR_REL}")"

  _asryx_log "installing ${dst_bin}"
  install -m 0755 "${src_bin}" "${dst_bin}"
}

_asryx_select_compiler() {
  if [[ -n "${CC:-}" && -n "${CXX:-}" ]]; then
    ASRYX_COMPILER_CC="${CC}"
    ASRYX_COMPILER_CXX="${CXX}"
    _asryx_log "compiler: $(${ASRYX_COMPILER_CXX} --version | sed -n '1p')"
    return 0
  fi

  if _asryx_have clang && _asryx_have clang++; then
    ASRYX_COMPILER_CC="clang"
    ASRYX_COMPILER_CXX="clang++"
  elif _asryx_have gcc && _asryx_have g++; then
    ASRYX_COMPILER_CC="gcc"
    ASRYX_COMPILER_CXX="g++"
  else
    _asryx_die "missing C++23 compiler: install clang++ or g++"
  fi

  export CC="${ASRYX_COMPILER_CC}"
  export CXX="${ASRYX_COMPILER_CXX}"
  _asryx_log "compiler: $(${ASRYX_COMPILER_CXX} --version | sed -n '1p')"
}

_asryx_select_generator() {
  if [[ -n "${CMAKE_GENERATOR:-}" ]]; then
    _asryx_log "generator: ${CMAKE_GENERATOR}"
    return 0
  fi

  if _asryx_have ninja; then
    export CMAKE_GENERATOR="Ninja"
    _asryx_log "generator: Ninja"
    return 0
  fi

  _asryx_log "generator: CMake default"
}

_asryx_configure_asryx() {
  local preset="$1"

  _asryx_log "configuring asryx (${ASRYX_BACKEND})"
  cmake \
    --fresh \
    --preset "${preset}" \
    -S "${ASRYX_REPO_DIR}" \
    -DCMAKE_C_COMPILER="${ASRYX_COMPILER_CC}" \
    -DCMAKE_CXX_COMPILER="${ASRYX_COMPILER_CXX}"
}

_asryx_compile_asryx() {
  local preset="$1"

  _asryx_log "building asryx (${ASRYX_BACKEND})"
  cmake --build "${ASRYX_REPO_DIR}/build/${preset}" --target asryx
}
