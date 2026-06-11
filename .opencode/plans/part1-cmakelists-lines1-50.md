# Part 1: `CMakeLists.txt` (lines 1–50)

## Overview

`CMakeLists.txt` is the root build file for the CMake build system. It defines the project, its requirements, dependencies, source files, compiler flags, include directories, linker libraries, shader compilation rules, and output layout. This part covers lines 1 through 50, which span project metadata, C++ standard configuration, the optional C++20 module toggle, dependency discovery, source file gathering, the executable target, compiler feature requirements, and include directories.

---

## Line-by-line breakdown

### Line 1
```cmake
cmake_minimum_required(VERSION 3.29)
```

**What it does:** Sets the minimum version of CMake required to process this file. If the installed CMake is older than 3.29, CMake will emit a fatal error and stop.

**Why 3.29:** This version is needed because:
- CMake 3.28+ improved support for C++20 modules via `CMAKE_CXX_SCAN_FOR_MODULES`
- CMake 3.29 added further refinements to module scanning
- The `file(GLOB ...)` pattern, `find_package`, `add_executable`, and `target_*` commands used throughout are stable in this version

**Every detail:**
- `cmake_minimum_required` is a CMake command, not a function — it's evaluated at parse time, before any other commands run
- The `VERSION` keyword is followed by a dotted triple `major.minor.patch`; here only `3.29` is specified, which is equivalent to `3.29.0`
- If this line is not the first command (comments and blank lines are allowed before it), CMake still processes it first conceptually
- After this line, CMake policies are set to the version specified; for 3.29, policy `CMP0148` (involving `FindVulkan`) might affect behavior

---

### Line 2
*(blank line)*

Pure whitespace for readability. No semantic effect.

---

### Line 3
```cmake
set(TEMPLATE_NAME "Chronica_Regna_Fractorum")
```

**What it does:** Defines a CMake variable named `TEMPLATE_NAME` with the string value `Chronica_Regna_Fractorum`.

**Every detail:**
- `set()` is a CMake command that creates or overwrites a variable in the current scope (directory scope, since this is in the root `CMakeLists.txt`)
- Variable names in CMake are case-sensitive; `TEMPLATE_NAME` is an arbitrary choice
- The value is a quoted string. CMake strings are not typed — everything is a string internally
- This variable is now available for `${TEMPLATE_NAME}` expansion later in the file
- Using a variable here so the project name is defined in exactly one place and reused for the `project()` call, the executable target name, and the output directory

---

### Line 4
```cmake
project(${TEMPLATE_NAME})
```

**What it does:** Declares the CMake project with the name resolved from `${TEMPLATE_NAME}`, i.e. `Chronica_Regna_Fractorum`.

**Every detail:**
- `project()` is a CMake command that:
  - Sets `PROJECT_NAME` to `Chronica_Regna_Fractorum`
  - Sets `CMAKE_PROJECT_NAME` to the same (since this is the top-level `CMakeLists.txt`)
  - Sets `PROJECT_SOURCE_DIR` and `PROJECT_BINARY_DIR` to the corresponding source/build directories
  - Without additional arguments, it does not explicitly set languages; CMake defaults to enabling `C` and `CXX` (C and C++). Since we only need C++, this is fine but slightly wasteful — we could add `LANGUAGES CXX` to be explicit
  - Triggers CMake to check for the C++ compiler
- The variable expansion `${TEMPLATE_NAME}` is resolved at command execution time, not parse time

---

### Line 5
*(blank line)*

---

### Line 6
```cmake
set(CMAKE_CXX_STANDARD 20)
```

**What it does:** Sets the CMake variable `CMAKE_CXX_STANDARD` to `20`, which tells CMake to request the C++20 standard from the compiler for all targets in this directory and below.

**Every detail:**
- `CMAKE_CXX_STANDARD` is a CMake *variable*, not a property. When set, it becomes the default value for the `CXX_STANDARD` property on every target
- The value is the integer `20`, representing the C++20 standard (ISO/IEC 14882:2020)
- CMake translates this to compiler flags like `-std=c++20` for GCC/Clang or `/std:c++20` for MSVC
- This variable only sets the *request*; whether the compiler actually uses C++20 depends on `CMAKE_CXX_STANDARD_REQUIRED` (line 7)
- Without line 7, if the compiler doesn't support C++20, CMake would silently fall back to the compiler's default

---

### Line 7
```cmake
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

**What it does:** Makes the C++ standard requirement *hard* — if the compiler cannot do C++20, CMake will emit a fatal error.

**Every detail:**
- `CMAKE_CXX_STANDARD_REQUIRED` is a boolean variable (`ON` / `OFF`)
- When `ON`, the `CXX_STANDARD` property value becomes a requirement; the compiler *must* support the requested standard
- When combined with `CMAKE_CXX_STANDARD 20`, this means "fail the configure step if the C++ compiler does not support C++20"
- This prevents silent fallback to C++17 or C++14, which would break code using C++20 features like `std::span`, `std::format`, designated initializers, template lambdas, `consteval`, etc.

---

### Line 8
*(blank line)*

---

### Line 9
```cmake
option(ENABLE_CPP20_MODULE "Enable C++ 20 module support for Vulkan" OFF)
```

**What it does:** Defines a user-togglable CMake option named `ENABLE_CPP20_MODULE`, defaulting to `OFF`.

**Every detail:**
- `option()` is a CMake command that creates a boolean variable with a doc string
- The first argument `ENABLE_CPP20_MODULE` is the variable name
- The second `"Enable C++ 20 module support for Vulkan"` is a description shown in CMake GUI tools (`ccmake`, `cmake-gui`)
- The third `OFF` is the default value
- Users can override it at configure time: `cmake -DENABLE_CPP20_MODULE=ON ..`
- This is currently `OFF`, meaning C++20 modules are not used. The project instead uses traditional `#include` headers
- The Vulkan C++ bindings (`vulkan.hpp`) support being compiled as a C++20 module via `VulkanCppModule`, but this is opted out by default because:
  - Module support in CMake is still maturing
  - Not all toolchains support it equally
  - The traditional include path is simpler and more portable

---

### Lines 10–13
```cmake
if(ENABLE_CPP20_MODULE)
    set(CMAKE_CXX_SCAN_FOR_MODULES ON)
endif()
```

**What it does:** If the option from line 9 is `ON`, enable CMake's module scanning for C++20 modules.

**Every detail:**
- `if(ENABLE_CPP20_MODULE)` evaluates the CMake variable — if it is `ON`, `TRUE`, `1`, or `YES`, the condition is true
- `set(CMAKE_CXX_SCAN_FOR_MODULES ON)` tells CMake to scan source files for C++20 `import` / `export` / `module` declarations
- This is required when using C++20 modules, because CMake must discover module dependencies at configure time
- The `endif()` closes the conditional block
- Since the option defaults to `OFF`, this block is skipped by default — no module scanning overhead

---

### Lines 15–17
```cmake
# -------------------------
# Dependencies
# -------------------------
```

Comments. CMake comments begin with `#` and extend to end of line. These serve as a visual section header separating the project setup from the dependency discovery section.

---

### Line 18
```cmake
find_package(glfw3 REQUIRED)
```

**What it does:** Locates the GLFW3 library on the system using CMake's package-finding mechanism.

**Every detail:**
- `find_package()` searches for a CMake package configuration file or a `Find<name>.cmake` module
- For `glfw3`, CMake ships with a `FindGLFW3.cmake` module (or GLFW itself installs a `glfw3Config.cmake`)
- The `REQUIRED` keyword means: if the package is not found, CMake will emit a fatal error and stop configuration
- On success, `find_package` sets variables like `glfw3_FOUND`, `GLFW3_LIBRARIES`, `GLFW3_INCLUDE_DIRS`, and creates an **imported target** `glfw` (note: not `glfw3`)
- The imported target `glfw` is what gets linked on line 58 of the file
- GLFW (Graphics Library Framework) provides:
  - Windowing and context creation
  - Input handling (keyboard, mouse, joystick)
  - Vulkan surface creation extension (`glfwCreateWindowSurface`)
  - No OpenGL dependency here because we pass `GLFW_NO_API` in `glfwWindowHint`

---

### Line 19
```cmake
find_package(glm REQUIRED)
```

**What it does:** Locates the GLM (OpenGL Mathematics) library on the system.

**Every detail:**
- `glm` is a header-only C++ mathematics library inspired by GLSL
- It provides vector types (`vec2`, `vec3`, `vec4`), matrix types (`mat4`), and transformation functions (`lookAt`, `perspective`, `rotate`, etc.)
- Because it's header-only, `find_package(glm)` mainly just finds the include directory and provides an imported target `glm::glm`
- `REQUIRED` means configuration fails if GLM is not installed
- GLM is used in this project for:
  - `Vertex` struct position (`glm::vec2`) and color (`glm::vec3`)
  - `UniformBufferObject` containing `glm::mat4` for model/view/projection matrices
  - Matrix math in the rotation/animation logic

---

### Line 20
```cmake
find_package(Vulkan 1.4.335 REQUIRED)
```

**What it does:** Locates the Vulkan SDK on the system, requiring at least version 1.4.335.

**Every detail:**
- `find_package(Vulkan ...)` uses CMake's built-in `FindVulkan.cmake` module
- The version `1.4.335` specifies the minimum Vulkan header version; `VK_HEADER_VERSION` must be >= 335
- `REQUIRED` makes failure fatal
- On success, this sets:
  - `Vulkan_FOUND`
  - `Vulkan_INCLUDE_DIR` (path to `vulkan/vulkan.h`)
  - `Vulkan_LIBRARY` (path to `vulkan-1.lib` on Windows or `libvulkan.so` on Linux)
  - Creates the imported target `Vulkan::Vulkan`
- Vulkan 1.4.335 corresponds to a specific Vulkan specification version that includes features needed by this project:
  - Vulkan 1.3 core features (dynamic rendering, synchronization2)
  - `VK_KHR_swapchain` extension
  - Vulkan 1.4 is the latest major API version (as of this writing)
  - The `.335` header version ensures the C++ bindings (`vulkan.hpp`, `vulkan_raii.hpp`) have the latest fixes and features

---

### Lines 22–24
```cmake
# -------------------------
# Source files
# -------------------------
```

Section header comments for the source file discovery section.

---

### Line 25
```cmake
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
```

**What it does:** Defines a variable `SRC_DIR` pointing to the `src/` subdirectory relative to the current source directory.

**Every detail:**
- `CMAKE_CURRENT_SOURCE_DIR` is a CMake-generated variable containing the absolute path to the directory where the current `CMakeLists.txt` lives — i.e., the `Code/` directory
- Appending `/src` gives the full path to the `Code/src/` folder
- Using `SRC_DIR` as a variable means we can reference the source directory multiple times without repeating the full path (anticipating future changes)

---

### Line 26
```cmake
set(HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/header)
```

**What it does:** Same pattern as line 25 but for the `header/` directory (`Code/header/`).

**Every detail:**
- This path points to `Code/header/` which contains the `vulkan/` subdirectory with all `.hpp` headers
- Note: the separate `Code/include/` directory (which contains only bundled GLFW headers) is *not* captured by this variable and is handled differently (it's not added to include paths explicitly — it's expected that GLFW's own CMake config provides the system GLFW include path)

---

### Line 27
*(blank line)*

---

### Lines 28–30
```cmake
file(GLOB VULKAN_SOURCES
    "${SRC_DIR}/vulkan/*.cpp"
)
```

**What it does:** Collects all `.cpp` files in `src/vulkan/` into a CMake list variable named `VULKAN_SOURCES`.

**Every detail:**
- `file(GLOB ...)` generates a list of file paths matching the glob expression
- The glob pattern `"${SRC_DIR}/vulkan/*.cpp"` expands to (e.g.) `/home/.../Code/src/vulkan/Application.cpp`, `.../Buffers.cpp`, `.../Commands.cpp`, `.../Device.cpp`, `.../Instance.cpp`, `.../SwapchainPipeline.cpp`, `.../TextureMapping.cpp`
- The result is stored as a semicolon-separated list in `VULKAN_SOURCES`
- **Important caveat:** `file(GLOB)` is evaluated at *configure time*. If new `.cpp` files are added to `src/vulkan/` without re-running CMake, they will not be picked up. This is why many projects prefer to list files explicitly. Here it's a trade-off for convenience — if you add a new source file, you must re-run `cmake` (not just `make`/`ninja`)
- This glob does *not* include `src/main.cpp` — that file is listed separately on line 33
- The `VULKAN_SOURCES` list is used on line 34 when constructing the executable

---

### Lines 32–35
```cmake
add_executable(${TEMPLATE_NAME}
    ${SRC_DIR}/main.cpp
    ${VULKAN_SOURCES}
)
```

**What it does:** Creates an executable build target named `Chronica_Regna_Fractorum` (from `${TEMPLATE_NAME}`) composed of `main.cpp` and all files matched by the glob above.

**Every detail:**
- `add_executable()` is a CMake command that defines a build target
- The first argument is the target name; this becomes the name of the compiled binary (with platform-specific suffix: `.exe` on Windows, no suffix on Linux/macOS)
- `${SRC_DIR}/main.cpp` — the single entry-point file
- `${VULKAN_SOURCES}` — expands to the 7 source files from the glob
- All 8 `.cpp` files will be compiled individually and then linked together into the final executable
- The target name `${TEMPLATE_NAME}` (i.e. `Chronica_Regna_Fractorum`) is used in subsequent `target_*` commands to associate properties with this target

---

### Line 37
```cmake
target_compile_features(${TEMPLATE_NAME} PRIVATE cxx_std_20)
```

**What it does:** Explicitly tells CMake that the target requires the C++20 standard, as a compile feature rather than a property.

**Every detail:**
- `target_compile_features()` associates compile features with a target
- `PRIVATE` means this requirement applies only to the target itself, not to targets that link against it (which is irrelevant here since this is an executable, not a library)
- `cxx_std_20` is a CMake meta-feature that indicates "compiler must support C++20"
- This is an alternative (or supplement) to setting `CMAKE_CXX_STANDARD` and `CMAKE_CXX_STANDARD_REQUIRED` on lines 6–7. Here it's being used as an additional explicit assertion on the target level
- Effectively, this is redundant with lines 6–7 — but it provides defense-in-depth: if someone changes the global defaults, this target will still demand C++20

---

### Lines 39–42
```cmake
# -------------------------
# Include dirs
# -------------------------
```

Section header for include directory configuration.

---

### Lines 42–50
```cmake
target_include_directories(${TEMPLATE_NAME}
    PRIVATE
        ${HEADER_DIR}
        ${HEADER_DIR}/vulkan
        external/stb
        external/tinygltf
        external/tinyobjloader
        external/VMA        # <-- IMPORTANT for vk_mem_alloc.h
)
```

**What it does:** Adds directories to the compiler's include path when building the `Chronica_Regna_Fractorum` target.

**Every detail:**
- `target_include_directories()` adds include search paths for a target
- `PRIVATE` means these paths are only used when compiling this target (not propagated to consumers)
- Each directory is resolved relative to `CMAKE_CURRENT_SOURCE_DIR` (the `Code/` directory), except for `HEADER_DIR` which is already an absolute path (since it was constructed from `CMAKE_CURRENT_SOURCE_DIR`)

**The include paths, one by one:**

1. **`${HEADER_DIR}`** → `Code/header/`
   - Allows `#include "../header/vulkan/Application.hpp"` from `src/*.cpp` files
   - The `..` relative path works because this directory is on the include path

2. **`${HEADER_DIR}/vulkan`** → `Code/header/vulkan/`
   - Allows `#include "Application.hpp"` directly (without the `vulkan/` prefix)
   - Though currently no source file uses this flat style — they all use the relative `../../header/vulkan/` path

3. **`external/stb`** → `Code/external/stb/`
   - Provides access to `stb_image.h` via `#include <stb_image.h>`
   - stb is a single-header library by Sean Barrett for image loading

4. **`external/tinygltf`** → `Code/external/tinygltf/`
   - Provides access to `tiny_gltf.h` for glTF 3D model file loading
   - Though not currently used in the `.cpp` files, it's included in preparation for model loading

5. **`external/tinyobjloader`** → `Code/external/tinyobjloader/`
   - Provides access to `tiny_obj_loader.h` for Wavefront OBJ file loading
   - Similarly prepared for future use

6. **`external/VMA`** → `Code/external/VMA/`
   - Provides access to `vk_mem_alloc.h` — the Vulkan Memory Allocator library
   - **This is critical:** `Buffers.hpp` and `Device.hpp` both `#include "../../external/VMA/vk_mem_alloc.h"`
   - VMA provides a high-level memory management layer on top of Vulkan's `vkAllocateMemory` / `vkBindBufferMemory`
   - The comment `# <-- IMPORTANT for vk_mem_alloc.h` emphasizes that this include path is essential for compilation

---

## Summary of Part 1

Lines 1–50 of `CMakeLists.txt` establish the entire build groundwork:
- Minimum CMake version 3.29
- C++20 as a hard requirement
- An optional (off-by-default) toggle for C++20 modules
- Three external package dependencies: GLFW (windowing), GLM (math), Vulkan 1.4.335+ (graphics API)
- Source file discovery via glob and explicit listing, collected into an executable target
- Seven include directories for headers, external single-header libraries, and VMA

The next part (lines 51–140) will cover linking those dependencies, shader compilation with slangc/glslc fallback, output directory configuration, and post-build shader copying.
