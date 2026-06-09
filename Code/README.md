# Technical Documentation — Chronica Regna Fractorum

## Background

This engine is built from the [Vulkan Tutorial](https://vulkan.org/) by Alexander Overvoorde. Several changes have been made to adapt to C++20 and Vulkan 1.4, since the original tutorial code uses older patterns that no longer compile cleanly.

## Memory Management

Every major Vulkan object in this codebase uses **`vk::raii`** wrappers ("RAII" = **R**esource **A**cquisition **I**s **I**nitialization). When an RAII object goes out of scope, its destructor automatically calls the appropriate Vulkan `vkDestroy*` / `vkFree*` function. This means **no manual cleanup** is required for individual resources — destruction is automatic and deterministic.

---

# `CMakeLists.txt` (Lines 1–50)

## Overview

`CMakeLists.txt` is the root build file for **CMake** (Cross-platform Make). It defines:
- what the project is called
- what compiler standard to use
- what external libraries it needs
- what source files to compile
- where to find headers
- how to link everything together
- how to compile shaders

---

## Line 1 — `cmake_minimum_required(VERSION 3.29)`

```cmake
cmake_minimum_required(VERSION 3.29)
```

### What it does

This command **sets the minimum CMake version** needed to build the project. If someone tries to configure with CMake **older than 3.29**, the build stops immediately with a fatal error. No other command in this file executes until this one passes.

### Why 3.29 specifically?

There are **three reasons** this version floor was chosen:

- **🧩 C++20 module support** — CMake 3.28 introduced the `CMAKE_CXX_SCAN_FOR_MODULES` feature, and 3.29 refined the scanning logic. The project has an optional (off-by-default) toggle for Vulkan C++ modules that depends on this.
- **🖥️ FindVulkan improvements** — Newer CMake versions ship with better `FindVulkan.cmake` modules that correctly locate the Vulkan SDK on Windows, Linux, and macOS.
- **🔄 Policy behavior** — CMake uses a policy system for backward compatibility. Setting `VERSION 3.29` implicitly sets all CMake policies to the `NEW` behavior from the 3.29 release, which matches what the rest of this file expects.

### Syntax breakdown

| Token | Meaning |
|-------|---------|
| `cmake_minimum_required` | A **CMake command** (not a function). It's evaluated at **parse time** before any other commands execute. |
| `VERSION` | A required **keyword argument** that introduces the version specifier. |
| `3.29` | The version number. Technically a triple `major.minor.patch`; here `3.29` is equivalent to `3.29.0`. |

> [!NOTE]
> If this line isn't the *first* command in the file, CMake moves it to the front conceptually anyway — it's always processed before anything else, even if blank lines or comments precede it.

### What happens if you don't have it?

Without this line, CMake would use its **default minimum version** (which is effectively `2.8` for most systems). The build might still work, but:
- some CMake features used later (like `Vulkan 1.4.335` version checking in `find_package`) might silently fail
- the policies would be set to the *installed* CMake's default, which could differ across developer machines
- **reproducibility** suffers — everyone should agree on a minimum version

---

## Line 3 — `set(TEMPLATE_NAME "Chronica_Regna_Fractorum")`

```cmake
set(TEMPLATE_NAME "Chronica_Regna_Fractorum")
```

Creates a CMake variable `TEMPLATE_NAME` containing the project name string. This is a **directory-scoped** variable — visible everywhere in this `CMakeLists.txt` and any subdirectories.

This one variable is consumed in three places:
- **Line 4** — `project(${TEMPLATE_NAME})`
- **Line 32** — `add_executable(${TEMPLATE_NAME} ...)`
- **Line 131** — output directory path

The pattern keeps the name in one place. If you rename the project, you change one line instead of three.

> [!NOTE]
> CMake `set()` always creates **string variables** (everything in CMake is a string). Variables are case-sensitive; `${TEMPLATE_NAME}` and `${template_name}` are different.

---

## Line 4 — `project(${TEMPLATE_NAME})`

```cmake
project(${TEMPLATE_NAME})
```

This is the **mandatory project declaration**. It tells CMake "this directory tree is a project named `Chronica_Regna_Fractorum`". The `project()` command sets several automatic variables:
- `PROJECT_NAME` → `Chronica_Regna_Fractorum`
- `CMAKE_PROJECT_NAME` → same (since this is the root `CMakeLists.txt`)
- `PROJECT_SOURCE_DIR` → the directory containing this file
- `PROJECT_BINARY_DIR` → the build directory

By default `project(...)` without `LANGUAGES` enables **both C and C++**. Since this project only uses C++, you could add `LANGUAGES CXX` to be explicit, but the default is harmless.

The `${TEMPLATE_NAME}` gets expanded before `project()` runs — the result is identical to writing `project(Chronica_Regna_Fractorum)`.

---

## Lines 6–7 — C++ standard enforcement

```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

**Line 6** requests the C++20 standard from the compiler. CMake translates this to the right compiler flag:
- GCC/Clang → `-std=c++20`
- MSVC → `/std:c++20`

**Line 7** makes this a **hard requirement**. Without it, if the compiler doesn't support C++20, CMake would silently fall back to the compiler's default — which could be C++17 or even C++14. That would break any C++20 features used in the code (designated initializers, `std::span`, etc.).

Together, these two lines mean: "**fail the build** if the compiler can't handle C++20."

---

## Line 9 — `option(ENABLE_CPP20_MODULE ...)`

```cmake
option(ENABLE_CPP20_MODULE "Enable C++ 20 module support for Vulkan" OFF)
```

An `option()` is a **boolean CMake variable** that users can toggle at configure time:
```bash
cmake -DENABLE_CPP20_MODULE=ON ..
```

The three arguments are:
1. **Variable name** — `ENABLE_CPP20_MODULE`
2. **Description** — shown in `cmake-gui` / `ccmake`
3. **Default** — `OFF`

This is off by default because C++20 module support across compilers and build systems is still uneven. The project uses traditional `#include` headers for the Vulkan C++ bindings instead.

### Lines 11–13
```cmake
if(ENABLE_CPP20_MODULE)
    set(CMAKE_CXX_SCAN_FOR_MODULES ON)
endif()
```

If someone turns the option `ON`, this block enables CMake's **module dependency scanner**. It tells CMake to scan source files for `import`, `export`, and `module` declarations so it can build the correct dependency graph. Skipped entirely when the option is `OFF`.

---

## Lines 18–20 — External dependencies (`find_package`)

```cmake
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(Vulkan 1.4.335 REQUIRED)
```

These three lines locate external libraries on the system. `find_package` searches CMake-config directories and system paths for each library. `REQUIRED` means **fail immediately** if not found.

### `glfw3` — Windowing + input + Vulkan surface

GLFW provides:
- Window creation (with `GLFW_NO_API` hint — no OpenGL context)
- Keyboard, mouse, joystick input
- `glfwCreateWindowSurface()` — the cross-platform way to create a Vulkan surface from an OS window

On success, CMake provides an **imported target** called `glfw` (not `glfw3`), linked on line 58.

### `glm` — Mathematics

GLM is a **header-only** C++ library that mirrors GLSL's vector/matrix types. This project uses:
- `glm::vec2` — vertex positions (2D)
- `glm::vec3` — vertex colors (RGB)
- `glm::mat4` — uniform buffer matrices (model/view/projection)

Because it's header-only, `find_package(glm)` really just finds the include path and provides the target `glm::glm`.

### `Vulkan 1.4.335` — The graphics API

This locates the Vulkan SDK with a **minimum header version** of 1.4.335. At this version:
- Vulkan 1.3 core features are guaranteed (dynamic rendering, synchronization2)
- The `VK_KHR_swapchain` extension is available
- The C++ bindings (`vulkan.hpp`, `vulkan_raii.hpp`) have the latest RAII wrappers

The imported target is `Vulkan::Vulkan`.

---

## Lines 25–26 — Source/header directory variables

```cmake
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/header)
```

`CMAKE_CURRENT_SOURCE_DIR` is a **built-in CMake variable** holding the absolute path to wherever this `CMakeLists.txt` lives (i.e. `.../Code/`). These two lines build absolute paths to `src/` and `header/`.

These are used later:
- `SRC_DIR` — for the glob pattern on line 29 and the explicit `main.cpp` on line 33
- `HEADER_DIR` — for include paths on lines 44–45

---

## Lines 28–30 — Source file glob

```cmake
file(GLOB VULKAN_SOURCES
    "${SRC_DIR}/vulkan/*.cpp"
)
```

`file(GLOB ...)` collects all files matching a wildcard pattern into a CMake list. This captures the 7 source files in `src/vulkan/`:
- `Application.cpp`
- `Buffers.cpp`
- `Commands.cpp`
- `Device.cpp`
- `Instance.cpp`
- `SwapchainPipeline.cpp`
- `TextureMapping.cpp`

> [!WARNING]
> `file(GLOB)` is evaluated at **configure time**, not build time. If you add a new `.cpp` to `src/vulkan/`, you must re-run `cmake ..` for it to be picked up. Just typing `make` won't see it.

`main.cpp` is intentionally excluded from this glob — it's listed separately on line 33.

---

## Lines 32–35 — Executable target

```cmake
add_executable(${TEMPLATE_NAME}
    ${SRC_DIR}/main.cpp
    ${VULKAN_SOURCES}
)
```

Creates the build target. This tells CMake: "compile these 8 `.cpp` files and link them into an executable named `Chronica_Regna_Fractorum`."

The executable name becomes:
- `Chronica_Regna_Fractorum.exe` on Windows
- `Chronica_Regna_Fractorum` on Linux/macOS

The `${VULKAN_SOURCES}` expands to all 7 files from the glob above.

---

## Line 37 — Extra C++20 feature requirement

```cmake
target_compile_features(${TEMPLATE_NAME} PRIVATE cxx_std_20)
```

This is redundant with lines 6–7 but provides **defense-in-depth**. Even if someone removes the global `CMAKE_CXX_STANDARD` setting, the target itself still demands C++20.

`PRIVATE` means the requirement applies only to this target (not propagated to anything that links against it — irrelevant here since it's an executable, not a library).

---

## Lines 42–50 — Include directories

```cmake
target_include_directories(${TEMPLATE_NAME}
    PRIVATE
        ${HEADER_DIR}
        ${HEADER_DIR}/vulkan
        external/stb
        external/tinygltf
        external/tinyobjloader
        external/VMA
)
```

Adds six paths to the compiler's `-I` (include) search list. `PRIVATE` means they only apply to this target.

| Path | Maps to | Used for |
|------|---------|----------|
| `${HEADER_DIR}` | `Code/header/` | project headers via `#include "../header/vulkan/..."` |
| `${HEADER_DIR}/vulkan` | `Code/header/vulkan/` | flat `#include "Application.hpp"` |
| `external/stb` | `Code/external/stb/` | `#include <stb_image.h>` — PNG/JPG loading |
| `external/tinygltf` | `Code/external/tinygltf/` | `#include <tiny_gltf.h>` — glTF 3D model loading |
| `external/tinyobjloader` | `Code/external/tinyobjloader/` | `#include <tiny_obj_loader.h>` — OBJ model loading |
| `external/VMA` | `Code/external/VMA/` | `#include "vk_mem_alloc.h"` — **Vulkan Memory Allocator** |

The `external/` paths are relative to `CMAKE_CURRENT_SOURCE_DIR` (the `Code/` directory).

The **VMA** entry is the most critical — it provides the memory allocator used by `Buffers.cpp` and `Device.cpp`. The comment `# <-- IMPORTANT for vk_mem_alloc.h` flags this explicitly.

---

# `CMakeLists.txt` (Lines 51–140) — Linking, Shaders, Output

---

## Lines 55–60 — Linking the libraries

```cmake
target_link_libraries(${TEMPLATE_NAME}
    PRIVATE
        Vulkan::Vulkan
        glfw
        glm::glm
)
```

This tells the linker to pull in three libraries when building the executable.

| Target | Library | What it provides |
|--------|---------|------------------|
| `Vulkan::Vulkan` | `vulkan-1.lib` (Win) / `libvulkan.so` (Linux) | All Vulkan API entry points, `vulkan.hpp` C++ bindings |
| `glfw` | `libglfw3.a` / `glfw3.lib` | Window creation, input, `glfwCreateWindowSurface` |
| `glm::glm` | header-only | No actual linking — just ensures the include path is correct |

The order matters for some linkers. Here, Vulkan comes first (it has no dependencies on the others), GLFW depends on the platform's windowing system, and GLM is header-only.

---

## Lines 65–67 — Vulkan compile definition

```cmake
target_compile_definitions(${TEMPLATE_NAME} PRIVATE
    VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
)
```

This defines a **preprocessor macro** that affects how the Vulkan C++ bindings handle the error `VK_ERROR_OUT_OF_DATE_KHR`. This error is returned by `vkAcquireNextImageKHR` when the swapchain needs to be recreated (e.g. after window resize). With this macro defined, the RAII wrapper treats that specific error as **equivalent to success** (`VK_SUCCESS`), letting the existing out-of-date handling logic take over instead of throwing an exception.

Without this define, an out-of-date swapchain would propagate as a `vk::OutOfDateKHRError` exception, which would need a separate catch block.

---

## Lines 72–81 — Optional C++20 Vulkan module (off by default)

```cmake
if(ENABLE_CPP20_MODULE)
    add_library(VulkanCppModule INTERFACE)
    add_library(Vulkan::cppm ALIAS VulkanCppModule)
    target_link_libraries(VulkanCppModule INTERFACE Vulkan::Vulkan)
    target_compile_definitions(VulkanCppModule INTERFACE
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
    )
endif()
```

When the `ENABLE_CPP20_MODULE` option is `ON`, this block creates an **interface library** (a header-only / compile-option-only library) called `VulkanCppModule`, aliased to `Vulkan::cppm`.

It sets two additional defines:

- **`VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1`** — Enables dynamic loading of Vulkan function pointers instead of linking directly. This is needed when using Vulkan as a C++20 module because the module version can't rely on static linking in the same way.
- **`VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1`** — Disables the C++ wrapper struct constructors, forcing the use of Vulkan's C-style struct initialization. When combined with designated initializers (C++20 feature), this produces cleaner code and avoids subtle initialization bugs.

Since the option is `OFF` by default, none of this applies unless explicitly enabled.

---

## Lines 86–125 — Shader compilation pipeline

```cmake
find_program(GLSLC glslc)
find_program(SLANGC slangc)
```

These search the system `PATH` for two shader compilers:
- **`slangc`** — The Slang shader compiler (preferred)
- **`glslc`** — The Google/GLGSL shader compiler (fallback)

### Lines 89–92 — Shader paths

```cmake
set(SHADER_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_DIR})
set(SHADER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/slang.slang)
```

- `SHADER_DIR` → `<build_dir>/shaders` — where compiled `.spv` files go
- `SHADER_SOURCE` → `<source_dir>/shaders/slang.slang` — the single shader source file

### Lines 94–125 — Compilation logic

```cmake
if(EXISTS ${SHADER_SOURCE})
    if(SLANGC)
        ...use slangc...
    elseif(GLSLC)
        ...use glslc as fallback...
    else()
        message(WARNING ...)
    endif()
else()
    message(STATUS "No shader source found...")
endif()
```

The logic chain:

1. **Check if `shaders/slang.slang` exists** — if not, just print a status message and skip. This allows building without shaders (useful for CI or partial builds).

2. **Prefer `slangc`** — If the Slang compiler is found, use it with SPIR-V specific flags:
   - `-target spirv` — Output SPIR-V binary
   - `-profile spirv_1_4` — Target SPIR-V 1.4 (required for Vulkan 1.2+ features like `VK_KHR_spirv_1_4`)
   - `-emit-spirv-directly` — Skip the intermediate reflection step
   - `-o` — Output file path

3. **Fall back to `glslc`** — If `slangc` isn't found but `glslc` is, use it. This requires the `.slang` file to also be valid GLSL, which isn't guaranteed — this fallback may fail if the shader uses Slang-specific syntax.

4. **Warn if neither is found** — `message(WARNING ...)` prints a warning but doesn't stop the build. The executable will still be compiled, but there will be no shader, so it can't render anything.

The `add_custom_command` with `DEPENDS ${SHADER_SOURCE}` ensures the shader is recompiled whenever `shader.slang` changes.

---

## Lines 130–132 — Output directory

```cmake
set_target_properties(${TEMPLATE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${TEMPLATE_NAME}
)
```

Puts the compiled executable into `<build_dir>/Chronica_Regna_Fractorum/` instead of the default `<build_dir>/`. This keeps the build tree organized and matches the pattern that Visual Studio and Xcode use for per-config output directories.

---

## Lines 135–139 — Post-build shader copy

```cmake
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders
            ${CMAKE_BINARY_DIR}/shaders
)
```

This runs **after every successful build** of the target. It copies the `shaders/` directory from source to the build directory using CMake's platform-portable `-E` (execute) mode. This ensures the compiled `.spv` file ends up next to the executable so the runtime can find it by the relative path `"shaders/slang.spv"`.

`POST_BUILD` means it always runs after linking, even if nothing changed. The overhead is minimal for a directory copy.

---

# Shader Compilation Scripts & The Shader Itself

---

## `compileShaders.sh` / `compileShaders.bat`

Manual shader compilation scripts (one for Linux/macOS, one for Windows) as an alternative to the CMake-built-in compilation.

```bash
# compileShaders.sh (Linux/macOS)
slangc shaders/shader.slang \
    -target spirv \
    -profile spirv_1_4 \
    -emit-spirv-directly \
    -fvk-use-entrypoint-name \
    -entry vertMain \
    -entry fragMain \
    -o shaders/slang.spv
```

```batch
:: compileShaders.bat (Windows)
slangc shaders\shader.slang ^
    -target spirv ^
    -profile spirv_1_4 ^
    -emit-spirv-directly ^
    -fvk-use-entrypoint-name ^
    -entry vertMain ^
    -entry fragMain ^
    -o shaders\slang.spv
```

These two scripts do the same thing in their respective shells. They differ from the CMake build in two ways:

| Flag | Purpose |
|------|---------|
| `-fvk-use-entrypoint-name` | Tells Slang to use the **function name** as the SPIR-V entry point (`vertMain`, `fragMain`) rather than generating generic names. This is required for Vulkan — the pipeline creation looks up entry points by name. |
| `-entry vertMain` / `-entry fragMain` | Explicitly lists which functions are entry points. The CMake build doesn't pass these because it was expected that slangc would auto-detect `[shader("vertex")]` attributes, but these scripts make it explicit. |

The CMake build skips `-fvk-use-entrypoint-name`. This means the CMake build is currently **less correct** than the manual scripts — the shader might not link to the pipeline correctly if slangc generated a mangled entry-point name. This is a bug in the CMake configuration.

---

## `shaders/shader.slang` — The shader source

```slang
struct VSInput {
    float2 inPosition;
    float3 inColor;
};

struct VSOutput {
    float4 pos   : SV_Position;
    float3 color : COLOR0;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos   = float4(input.inPosition, 0.0, 1.0);
    output.color = input.inColor;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    return float4(vertIn.color, 1.0);
}
```

### What this shader does

A **pass-through** shader pair — the vertex shader takes 2D positions + RGB colors, passes them through to the fragment shader, which outputs the color directly. No lighting, no texturing, no transformation.

### `VSInput` — Vertex input from C++

Mirrors the `Vertex` struct in `Buffers.hpp`:
| Member | C++ type | Slang type | Purpose |
|--------|----------|------------|---------|
| `inPosition` | `glm::vec2` | `float2` | 2D vertex position (x, y) |
| `inColor` | `glm::vec3` | `float3` | RGB color (r, g, b) |

### `VSOutput` — Output struct with semantics

- **`SV_Position`** — A **system-value semantic**. Tells the GPU this float4 is the clip-space position. Required for vertex shader output.
- **`COLOR0`** — A user-defined semantic. Passes the color to the fragment shader. The `0` allows multiple color outputs.

### `vertMain` — Vertex shader entry point

```slang
output.pos   = float4(input.inPosition, 0.0, 1.0);
```

Takes the 2D `(x, y)` position and constructs a 4D homogeneous vector:
- `x`, `y` — from the vertex data
- `z = 0.0` — no depth
- `w = 1.0` — homogeneous coordinate (standard for positions)

This means all vertices are at z=0 in clip space, rendering a flat 2D shape.

### `fragMain` — Fragment shader entry point

```slang
return float4(vertIn.color, 1.0);
```

Takes the interpolated color from the vertex shader and outputs it with **alpha = 1.0** (fully opaque). The `: SV_TARGET` semantic marks this as the render target output.

---

# External Libraries

Located in `Code/external/`. These are **copied directly into the repository** rather than fetched as system packages, because they're single-header libraries that don't need compilation.

| Directory | File | Purpose | Used by |
|-----------|------|---------|---------|
| `VMA/` | `vk_mem_alloc.h` | **Vulkan Memory Allocator** — manages GPU memory allocations, pooling, defragmentation. Replaces raw `vkAllocateMemory` calls. | `Device.cpp` (creates the allocator), `Buffers.cpp` (creates vertex/index/uniform buffers) |
| `stb/` | `stb_image.h` | **Sean Barrett's image loader** — decodes PNG, JPEG, BMP, etc. into raw pixel arrays. Single-header, public domain. | `TextureMapping.cpp` (`stbi_load`, `stbi_image_free`) |
| `tinygltf/` | `tiny_gltf.h` | **glTF 2.0 model loader** — reads glTF (GL Transmission Format) files including meshes, materials, animations, textures. | Not yet used in source, but the include path is set up. |
| `tinyobjloader/` | `tiny_obj_loader.h` | **Wavefront OBJ model loader** — reads the classic .obj format (vertices, normals, UVs, faces). | Not yet used in source, but the include path is set up. |

### VMA — the Vulkan Memory Allocator

This is the most significant external dependency. In `Device.cpp`, the line:

```cpp
#define VMA_IMPLEMENTATION
#include "../../external/VMA/vk_mem_alloc.h"
```

The `#define VMA_IMPLEMENTATION` must be defined in **exactly one** translation unit before including the header. This tells VMA to generate its implementation code (function bodies) rather than just declarations. `Device.cpp` is that translation unit.

VMA wraps Vulkan memory management into a simpler API:
- `vmaCreateBuffer()` — allocates both the `VkBuffer` and its backing `VkDeviceMemory` in one call
- `vmaMapMemory()` / `vmaUnmapMemory()` — maps GPU memory for CPU access
- `VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT` — optimization flag for CPU-to-GPU data transfers
- `VMA_ALLOCATION_CREATE_MAPPED_BIT` — keeps the memory persistently mapped

---

# Directory Structure (Complete Reference)

```
Code/                                    ← CMAKE_CURRENT_SOURCE_DIR
├── CMakeLists.txt                       ← Build system (this file)
├── compileShaders.sh                    ← Linux shader compilation
├── compileShaders.bat                   ← Windows shader compilation
│
├── src/                                 ← Source files
│   ├── main.cpp                         ← Entry point
│   └── vulkan/                          ← All Vulkan implementation
│       ├── Application.cpp              ← App lifecycle orchestration
│       ├── Instance.cpp                 ← Vulkan instance & surface
│       ├── Device.cpp                   ← GPU selection & logical device
│       ├── SwapchainPipeline.cpp        ← Swapchain & graphics pipeline
│       ├── Buffers.cpp                  ← Vertex/index/uniform buffers
│       ├── Commands.cpp                 ← Command buffers & rendering
│       └── TextureMapping.cpp           ← Texture loading
│
├── header/vulkan/                       ← Header files (mirrors src/)
│   ├── Application.hpp
│   ├── Instance.hpp
│   ├── Device.hpp
│   ├── SwapchainPipeline.hpp
│   ├── Buffers.hpp
│   ├── Commands.hpp
│   └── TextureMapping.hpp
│
├── include/GLFW/                        ← Bundled GLFW headers (platform)
│
├── external/                            ← Single-header libraries
│   ├── stb/stb_image.h
│   ├── VMA/vk_mem_alloc.h
│   ├── tinygltf/tiny_gltf.h
│   └── tinyobjloader/tiny_obj_loader.h
│
├── shaders/
│   └── shader.slang                     ← Slang shader source
│
├── textures/
│   └── texture.jpg                      ← Sample texture
│
├── libraries/
│   └── vulkan-1.lib                     ← Windows static import lib
│
├── slang/                               ← Empty (reserved for generated files)
│
└── README.md                            ← This file
```

That's everything that isn't C++ code.

---

# Documentation Template

Use this structure for every file:

```markdown
## `filename.hpp` / `filename.cpp`

<file summary — one or two sentences>

### Line N — `<code>`

- **`<symbol>`** — <what it does, why it's here>
- **`<symbol>`** — <connections to other parts of the code>
```

> [!TIP]
> - Use `**bold**` for key terms and variable names
> - Use `` `inline code` `` for code symbols
> - Use `> [!NOTE]` for side details
> - Use `> [!WARNING]` for pitfalls

## `main.cpp` 

<file summary — one or two sentences>

### Line N — `<code>`

- **`<symbol>`** — <what it does, why it's here>
- **`<symbol>`** — <connections to other parts of the code>
