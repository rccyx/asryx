set shell := ["bash", "-eu", "-o", "pipefail", "-c"]

alias f := format
alias fc := format-check
alias l := lint
alias b := build
alias t := test
alias sc := shellcheck
alias s := sanitizers
alias c := clean

cpp_sources := "find src tests -type f \\( -name '*.cpp' -o -name '*.hpp' -o -name '*.c' -o -name '*.h' \\) -print0"
tidy_sources := "find src tests -type f \\( -name '*.cpp' -o -name '*.hpp' \\) -print0"

@format:
	{{cpp_sources}} | xargs -0 -r clang-format -i

@format-check:
	{{cpp_sources}} | xargs -0 -r clang-format --dry-run --Werror

@test:
	just build
	ctest --preset release

@lint:
	python3 scripts/checks/check-line-limits.py
	python3 scripts/checks/check-module-boundaries.py
	python3 scripts/checks/check-owned-paths.py
	cmake --fresh --preset release
	{{tidy_sources}} | xargs -0 -r clang-tidy -p build/release
	cppcheck --enable=all --error-exitcode=1 --inline-suppr --suppress=checkersReport --suppress=missingInclude --suppress=normalCheckLevelMaxBranches --suppressions-list=cppcheck.suppressions --std=c++20 -I src -I tests -I . src tests

@shellcheck:
	shellcheck -x scripts/install scripts/uninstall scripts/lib/_common.sh scripts/lib/_deps.sh

@build:
	cmake --fresh --preset release
	cmake --build --preset release

@sanitizers:
	cmake --fresh --preset asan
	cmake --build --preset asan
	ctest --preset asan
	cmake --fresh --preset ubsan
	cmake --build --preset ubsan
	ctest --preset ubsan

@clean:
	rm -rf build .cache .asryx-test-home .asryx-test-runtime