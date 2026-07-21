# GPU builds

This uses the CPU by default:

GPU support is selected when the program is compiled.

The selected build replaces the installed binary at:

```text
~/.local/bin/asryx
```

So you won't have to run the uninstaller, just install again, it will cache everything and build with the new flags.

## CUDA

Install these packages:

```bash
sudo apt update
sudo apt install nvidia-driver nvidia-cuda-toolkit
```

You need:

```text
A working NVIDIA driver.
A compatible CUDA toolkit.
nvcc available through $PATH, or CUDACXX pointing to the CUDA compiler.
```

Build and install:

```bash
./package/install --cuda
```

## Vulkan

```bash
sudo apt update
sudo apt install libvulkan-dev glslc spirv-headers
```

Required:

```text
A working Vulkan driver and loader.
Vulkan development headers and loader library.
glslc available through PATH or VULKAN_SDK/bin.
SPIR-V headers required by the upstream GGML Vulkan build.
```

Build and install:

```bash
./package/install --vulkan
```
