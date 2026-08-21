set shell := ["bash", "-eu", "-o", "pipefail", "-c"]

alias f := format
alias fc := format-check
alias l := lint
alias b := build
alias bv := build-vulkan
alias bc := build-cuda
alias t := test
alias sc := shellcheck
alias i := install
alias idv := install-dev
alias u := uninstall
alias s := sanitizers
alias c := clean

cpp_sources := "find src tests -type f \\( -name '*.cpp' -o -name '*.hpp' -o -name '*.c' -o -name '*.h' \\) -print0"
src_tidy_sources := "find src -type f -name '*.cpp' -print0"
test_tidy_sources := "find tests -type f -name '*.cpp' -print0"
tidy_jobs := "2"
package_shell_sources := "find package -type f \\( -name '*.sh' -o -name 'build' -o -name 'install' -o -name 'uninstall' \\) -print0"

@format:
	{{cpp_sources}} | xargs -0 -r clang-format -i

@format-check:
	{{cpp_sources}} | xargs -0 -r clang-format --dry-run --Werror

@test:
	CC=clang CXX=clang++ cmake --fresh --preset test
	cmake --build --preset test --target asryx_tests
	ctest --preset test

@install:
	bash ./package/install

# you can run with the --dev flag with either --cuda or --vulkan options also
@install-dev:
	bash ./package/install --dev

@uninstall:
	bash ./package/uninstall

@lint:
	python3 lint/check-line-limits.py
	python3 lint/check-module-boundaries.py
	python3 lint/check-owned-paths.py
	CC=clang CXX=clang++ cmake --fresh --preset release
	CC=clang CXX=clang++ cmake --fresh --preset test
	{{src_tidy_sources}} | xargs -0 -r -n 1 -P "{{tidy_jobs}}" clang-tidy --config-file=.clang-tidy -p build/release
	{{test_tidy_sources}} | xargs -0 -r -n 1 -P "{{tidy_jobs}}" clang-tidy --config-file=.clang-tidy -p build/test
	cppcheck --enable=all --error-exitcode=1 --inline-suppr --suppress=checkersReport --suppress=missingInclude --suppress=normalCheckLevelMaxBranches --suppressions-list=cppcheck.suppressions --std=c++23 -I src -I tests -I . src tests

@shellcheck:
	{{package_shell_sources}} | xargs -0 -r shellcheck -x

@build:
	CC=clang CXX=clang++ bash ./package/build

@build-cuda:
	CC=clang CXX=clang++ bash ./package/build --cuda

@build-vulkan:
	CC=clang CXX=clang++ bash ./package/build --vulkan


@sanitizers:
	CC=clang CXX=clang++ cmake --fresh --preset asan
	cmake --build --preset asan --target asryx_tests
	ctest --preset asan
	CC=clang CXX=clang++ cmake --fresh --preset ubsan
	cmake --build --preset ubsan --target asryx_tests
	ctest --preset ubsan

@clean:
	rm -rf build .cache .asryx-test-home .asryx-test-runtime
