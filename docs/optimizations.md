# Optimizations

You don't have to do this, BUT:

If you want to speed up build times or squeeze a bit of inference performance out of the CPU, you can install these optional tools.

## OpenMP

A set of compiler pragmas and runtime libraries for high-performance multithreaded computing. It provides GGML with tighter loop scheduling and can boost throughput on multi-core CPUs by parallelizing tensor operations directly.

- **Debian / Ubuntu:** `sudo apt install libomp-dev`
- **Arch Linux:** `sudo pacman -S openmp`
- **Fedora:** `sudo dnf install libomp-devel`

## ccache

Caches compiled object files (`.o`). The initial build compiles normally so that subsequent builds and branch switches instantly swap in cached objects.

- **Debian / Ubuntu:** `sudo apt install ccache`
- **Arch Linux:** `sudo pacman -S ccache`
- **Fedora:** `sudo dnf install ccache`

## mold

The default GNU linker (`ld`) creates heavy link step bottlenecks. `mold` parallelizes the link pass to accelerate execution by up to 10x.

- **Debian / Ubuntu:** `sudo apt install mold` (needs Ubuntu 22.04+ or Debian 12+, older releases don't carry it in default repos)
- **Arch Linux:** `sudo pacman -S mold`
- **Fedora:** `sudo dnf install mold`
