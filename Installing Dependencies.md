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

## VulkanSDK

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

#### Arch

Installing Vulkan Validation layers:
```bash
pacman -S vulkan-headers vulkan-tools vulkan-validation-layers
```
The following *might* be worth installing. Check if the game runs after installing above.
```bash
pacman -S vulkan-icd-loader vulkan-intel vulkan-mesa-implicit-layers
```

## GLFW and GLM libraries

### Windows & Arch

Run the following commands:
```bash
pacman -S mingw-w64-x86_64-glfw
pacman -S mingw-w64-x86_64-glm
```

Then add the following to [path](https://www.youtube.com/watch?v=Z2k7ZBMZT3Y):
```
C:\msys64\mingw64\bin
```