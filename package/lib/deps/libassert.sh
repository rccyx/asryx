#!/usr/bin/env bash

# shellcheck disable=SC2034

DEP_NAME="libassert"
DEP_REPO="https://github.com/jeremy-rifkin/libassert.git"
DEP_REV="bd33ba116f209bf71761c58dccc2f3bf277e0824"
DEP_DIR_REL="${ASRYX_LIBASSERT_DIR_REL}"

dep_build() {
  rm -rf -- "${DEP_BUILD_DIR}" "${DEP_INSTALL_DIR}"

  _asryx_log "configuring ${DEP_NAME}"
  cmake -S "${DEP_SOURCE_DIR}" -B "${DEP_BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${DEP_INSTALL_DIR}" \
    -DLIBASSERT_BUILD_TESTING=OFF \
    -DLIBASSERT_USE_EXTERNAL_CPPTRACE=OFF

  _asryx_log "installing ${DEP_NAME}"
  cmake --build "${DEP_BUILD_DIR}"
  cmake --install "${DEP_BUILD_DIR}"
}
