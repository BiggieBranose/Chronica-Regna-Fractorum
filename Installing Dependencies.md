# Installing dependencies

Quick quide to installing dependencies, libraries and other necessesary files that for one reason or another cannot be added to the project on Git

## Installing Windows C++ Compiler
It might be worth installing [MSYS2](https://www.msys2.org) on Windows and run the following in MSYS2 MSYS (Remember to not be on the IKT-Agder_Intern network):
```bash
pacman -Syu
pacman -S mingw-w64-x86_64-gcc
```

Then in GitBash, paste:
```bash
nano ~/.bashrc
```
And in the file paste:
```bash
export PATH=/c/msys64/mingw64/bin:$PATH
```
Then exit the file, and paste in the following in GitBash:
```bash
source ~/.bashrc
gcc --version
```
In VSCode, run the CMAKE `scan for kits` command, and add `GCC` as VSCode compiler

## Linux C++ Compiler
Install the compiler using your system package manager:

### Debian / Ubuntu
```bash
sudo apt update
sudo apt install gcc g++ build-essential
```

### Arch / Manjaro
```bash
sudo pacman -S gcc make pkg-config
```

### Fedora
```bash
sudo dnf install gcc gcc-c++ make
```

Then verify the installation:
```bash
gcc --version
g++ --version
```

In VSCode, run the CMAKE `scan for kits` command, and select the installed `GCC` as the compiler.

## Build tools (CMake and Ninja)

### Arch / Manjaro
```bash
sudo pacman -S cmake ninja
```

### Debian / Ubuntu
```bash
sudo apt install cmake ninja-build
```

### Fedora
```bash
sudo dnf install cmake ninja-build
```

## VulkanSDK

### Windows

Install the VulkanSDK from [**vulkan.lunarg.org/sdk/home**](https://vulkan.lunarg.com/sdk/home)

Remember to set it to PATH. E.g. `C:\VulkanSDK\1.4.341.1\Bin`

> ### Windows
> Download the latest x64/x86 SDK installer
>
> Install the SDK in the default assigned directory
>
> Select the following components:
> - [x] GLM Headers
> - [x] SDL libraries and headers
> - [x] Shader Toolchain Debug Symbols - 64 bit
> - [x] Vulkan Memory Allocator header
> - [x] ARM64 binaries for cross compiling

### Linux

Install the Vulkan SDK through your system package manager, or download it from LunarG:

#### Debian / Ubuntu
```bash
sudo apt install vulkan-sdk
```

#### Arch / Manjaro

The Vulkan SDK is split across multiple packages on Arch. Install them directly:

```bash
sudo pacman -Syy --needed vulkan-headers vulkan-icd-loader spirv-headers
```

The `-y` refreshes the mirror list, and the double `-yy` forces a full refresh if you get 404 errors from a stale mirror. If you prefer the `vulkan-devel` meta-group instead, selecting members `3 7 8 11` gives you the same set (vulkan-icd-loader, vulkan-validation-layers, spirv-headers, vulkan-headers).

Optional extras:
- `vulkan-tools` — provides `vulkaninfo` for diagnosing GPU issues
- `vulkan-validation-layers` — validation layers for development/debugging
- `spirv-tools` — SPIR-V compilation tools

#### Fedora
```bash
sudo dnf install vulkan-devel
```

### Shader compiler (required — the engine needs a working `slangc` to compile shaders and display graphics)

The `slang` package in Arch repos is S-Lang (a scripting language), not the NVIDIA Slang shader compiler. Download the official release from GitHub:

```bash
cd /tmp

wget https://github.com/shader-slang/slang/releases/latest/download/slang-linux-x86_64-glibc-2.27.tar.gz

tar -xzf slang-linux-x86_64-glibc-2.27.tar.gz

sudo mv slang-linux-x86_64-glibc-2.27 /opt/slang
sudo ln -sf /opt/slang/bin/slangc /usr/local/bin/slangc
```

Verify the installation:

```bash
slangc --version
```

If `wget` fails, replace `wget` with `curl -LO` or download from [github.com/shader-slang/slang/releases](https://github.com/shader-slang/slang/releases) manually.

The SDK headers and libraries should be available on your system path automatically.

## GLFW and GLM libraries

### Windows

Run the following commands:
```bash
pacman -S mingw-w64-x86_64-glfw
pacman -S mingw-w64-x86_64-glm
```

Then add the following to [path](https://www.youtube.com/watch?v=Z2k7ZBMZT3Y):
```
C:\msys64\mingw64\bin
```

### Linux

Install the libraries using your system package manager:

#### Debian / Ubuntu
```bash
sudo apt install libglfw3-dev libglm-dev
```

#### Arch / Manjaro
```bash
sudo pacman -S glfw-x11 glm
```

#### Fedora
```bash
sudo dnf install glfw-devel glm-devel
```