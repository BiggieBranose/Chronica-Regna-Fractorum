<p align="center">
  <img src="gitAssets/Title.gif" alt="Chronica Regna Fractorum Title">
</p>

<p align="center">
  <i>A 2.5D fantasy RPG focused on immersion, technical depth, and deliberate design.</i>
</p>

---

<div align="center">

![Status](https://img.shields.io/badge/status-in%20development-3a6ea5)
![Engine](https://img.shields.io/badge/engine-custom%20vulkan-8a5cf6)
![Platform](https://img.shields.io/badge/platform-cross--platform-2f855a)

</div>

---

## <img src="gitAssets/overview.gif" width="300px" alt="Overview">

**Chronica Regna Fractorum** is a fantasy RPG built around a stylized 2.5D world, turn-based combat, and party-driven systems.

The focus is not on scale for its own sake, but on cohesion — building a world that feels internally consistent, responsive, and worth engaging with over time.

Rather than relying on abstraction-heavy tools, much of the project is approached from a lower level, allowing for tighter control over how systems behave and interact.

> **Note**: To run the project, build it first — it is scripted for you:
>
> ```bash
> # Windows (PowerShell / cmd):
> ./compile.bat
>
> # Git Bash / MSYS2:
> ./compile.sh
>
> # Re-run the last build without rebuilding:
> ./run.sh
> ```

---

## Getting Started

### Requirements

A working build needs:

- **Vulkan SDK** — [vulkan.lunarg.com](https://vulkan.lunarg.com/sdk/home)
- **C++20 compiler** — MinGW (`x86_64-w64-mingw32-gcc`) via [MSYS2](https://www.msys2.org) on Windows
- **CMake** and **Ninja**
- **GLFW** and **GLM** (installed via `pacman -S mingw-w64-x86_64-glfw mingw-w64-x86_64-glm`)

See [`Installing Dependencies.md`](Installing%20Dependencies.md) for the full setup guide.

### Controls

The camera is a fixed side-on follow view by default. Pass `--camera` (or `-c`) to unlock free-camera controls — hold **right mouse button** to orbit, scroll to zoom:

```bash
./run.sh --camera
```

Press **ESC** to exit.

---

## Project Structure

```
Code/
  game/            game logic: entry point, Game class, player setup
  engine/
    core/          types, math (Transform/AABB), logging, config
    physics/       collision world: colliders (col_*), triggers (trg_*)
    graphics/      Vulkan rendering, glTF loading, scene system
  shaders/         GLSL compiled to SPIR-V at build time
tools/             asset generators (Python)
assets/            models, textures
```

`Scene` loads a `.glb` scene file and interprets node names by prefix: `col_*` become static colliders, `trg_*` become trigger volumes, and everything else is rendered as a visible entity.

---

## Current Progress

Implemented so far:

- Custom **Vulkan** renderer — render passes, MSAA, depth buffering, shaders compiled from GLSL at build time
- glTF (`.glb`) scene loading with textures, node transforms and bounding boxes
- **Skybox** rendering
- **Scene system** — entities, per-node colliders (`col_*`) and trigger volumes (`trg_*`)
- **Physics** module — circle-vs-AABB and AABB-vs-AABB queries, trigger overlap detection
- Asset pipeline scripts for generating test meshes and scenes (`tools/`)

---

## <img src="gitAssets/project_direction.gif" width="300px" alt="Project Direction">

The intention is to create a world that feels grounded in its own rules — something that invites interaction and rewards attention to detail.

Immersion is treated as a result of consistency, not spectacle. Systems are designed to support that idea, even if it means taking longer to build them properly.

There is also a realistic understanding that the scope is ambitious.

*(progress permitting)*

---

## <img src="gitAssets/team.gif" width="300px" alt="Team">

<div align="center">

**BiggieBranose**  
Lead_Dev  
<sub>Johannes</sub>

<br>

**Viko-w**  
Dev  
<sub>Amadeus</sub>

<br>

**Thegyew**  
Dev  
<sub>Markus</sub>

<br>

**Mad S**  
Music_Individual  
<sub>Mads</sub>

<br>

**Stan**  
Emotional_Opposer  
<sub>Stian</sub>

<br>

**Tedoj**  
Designer *(mayhaps...)*  
<sub>Theodor</sub>

</div>

---

## <img src="gitAssets/rendering.gif" width="300px" alt="Rendering">

### Vulkan

Vulkan is used to maintain direct control over the rendering pipeline and to better understand how modern graphics systems operate beneath higher-level abstractions.

This approach allows for:

- deliberate control over performance-critical paths  
- explicit handling of GPU resources  
- the ability to tailor systems to the needs of the project rather than adapting to constraints  

The decision is as much educational as it is practical.

---

> [!IMPORTANT]
> **OpenGL**
>
> OpenGL was considered early on due to its accessibility and faster development cycle.  
> However, it was ultimately set aside due to limitations in performance control and architectural flexibility relative to the project's goals.
> 
> **DirectX**
>
> DirectX was avoided primarily due to platform constraints.  
> Maintaining cross-platform compatibility is treated as a core requirement, and avoiding ecosystem lock-in is part of that decision.

---

## <img src="gitAssets/documentation.gif" width="300px" alt="Documentation">

<div align="center">

<a href="https://github.com/BiggieBranose/Chronica-Regna-Fractorum/wiki">
  <kbd>
    <br>
    &nbsp;&nbsp;View Wiki&nbsp;&nbsp;
    <br><br>
  </kbd>
</a>

The wiki will gradually expand to include technical breakdowns, development notes, and internal system documentation as the project evolves.

</div>

---

## <img src="gitAssets/development_context.gif" width="300px" alt="Development Context">

This project serves a dual purpose: building a game, and building understanding.

Current areas of focus include:

- low-level graphics programming  
- engine structure and architecture  
- system-level experimentation  

As a result, progress may be uneven, and parts of the project will change or be reworked over time.

> [!NOTE]
> This is a hobby project developed alongside school and work.
>  
> Progress may be inconsistent, and timelines are not strictly defined. Development is driven by availability and long-term interest rather than deadlines.

---

<div align="right">

<a href="#chronica-regna-fractorum">
  <kbd>
    <br>
    &nbsp;&nbsp;Back to Top&nbsp;&nbsp;
    <br><br>
  </kbd>
</a>

</div>
