#!/usr/bin/env bash

# shellcheck disable=SC2034

DEP_NAME="whisper.cpp"
DEP_REPO="https://github.com/ggml-org/whisper.cpp.git"
DEP_REV="fc674574ca27cac59a15e5b22a09b9d9ad62aafe"
DEP_DIR_REL="${ASRYX_WHISPER_DIR_REL}"
DEP_READY_PATH_REL="${ASRYX_WHISPER_DIR_REL}/CMakeLists.txt"
DEP_SYNC_SUBMODULES=1
