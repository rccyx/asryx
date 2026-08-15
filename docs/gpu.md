# GPU Acceleration Setup

To run this with GPU support, you need your GPU's compiler and system libraries installed on your system before running the installer.

Prefer CUDA if you have NVIDIA, and Vulkan for the rest, AMD, and Intel (also NVIDIA).

Pass either `--cuda` or `--vulkan` directly to the installer:

```bash
bash ./package/install --vulkan
```

or

```bash
bash ./package/install --cuda
```

If you don't have the correct packages it will error out like this:

```
bash ./package/install --cuda

asryx install: error: missing required tools:
  - nvcc

install them with your system package manager and rerun ./package/install
```

Here's how to get them:

## CUDA Setup

You need the official NVIDIA proprietary driver, the CUDA toolkit to compile code via the `nvcc` compiler, and the compiler must be reachable through your system's `PATH` or `CUDACXX` environment variable.

On Arch Linux:

```bash
sudo pacman -S nvidia nvidia-utils cuda
```

On Debian:

```bash
sudo apt update
sudo apt install nvidia-driver nvidia-cuda-toolkit
```

On Fedora (requires RPM Fusion Non-Free repository):

```bash
sudo dnf install \
  https://mirrors.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm \
  https://mirrors.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-$(rpm -E %fedora).noarch.rpm

sudo dnf install akmod-nvidia xorg-x11-drv-nvidia-cuda
```

### Verify Your Driver

Before building, verify your system detects the CUDA setup:

```bash
nvidia-smi
```

This should print the GPU model, driver version, and CUDA version.

## Vulkan Setup

Vulkan requires a modern graphics driver with an **ICD configuration file** (tells the OS that the GPU handles Vulkan requests), the **Vulkan loader** (middleman lib that routes commands to the driver), the **development headers** (code definition files needed only during compilation), **`glslc`** (the shader compiler that translates raw shading instructions), and **SPIR-V headers** (standard layout files for binary shader packages used by GGML's backend).

On Arch Linux:

```bash
sudo pacman -S vulkan-icd-loader vulkan-headers vulkan-tools shaderc spirv-tools spirv-headers
```

On Debian or Ubuntu:

```bash
sudo apt update
sudo apt install libvulkan-dev glslc spirv-headers
```

On Fedora:

```bash
sudo dnf install vulkan-loader-devel vulkan-headers glslc spirv-headers-devel
```

### Verify Your Driver

```bash
vulkaninfo --summary
```

This should list your GPU as a physical device along with the supported Vulkan API version.
