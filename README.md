<h1 align="center">Chronica Regna Fractorum</h1>

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

## Overview

**Chronica Regna Fractorum** is a fantasy RPG built around a stylized 2.5D world, turn-based combat, and party-driven systems.

The focus is not on scale for its own sake, but on cohesion — building a world that feels internally consistent, responsive, and worth engaging with over time.

Rather than relying on abstraction-heavy tools, much of the project is approached from a lower level, allowing for tighter control over how systems behave and interact.

---

## Project Direction

The intention is to create a world that feels grounded in its own rules — something that invites interaction and rewards attention to detail.

Immersion is treated as a result of consistency, not spectacle. Systems are designed to support that idea, even if it means taking longer to build them properly.

There is also a realistic understanding that the scope is ambitious.

*(progress permitting)*

---

## Team

| Name | Role |
|------|------|
| **BiggieBranose (Johannes)** | Lead_Dev |
| **Viko-w (Amdus / Amandus / Amadeus)** | Dev |
| **Thegyew (Markus)** | Graphics_Dev |
| **Mad S (Mads)** | DjMusicMan |
| **Stan (Stian)** | Emotional_Opposer |
| **Tedoj (Theodor)** | Designer *(mayhaps...)* |

---

## Rendering

### Vulkan

Vulkan is used to maintain direct control over the rendering pipeline and to better understand how modern graphics systems operate beneath higher-level abstractions.

This approach allows for:

- deliberate control over performance-critical paths  
- explicit handling of GPU resources  
- the ability to tailor systems to the needs of the project rather than adapting to constraints  

The decision is as much educational as it is practical.

---

### OpenGL

OpenGL was considered early on due to its accessibility and faster development cycle.

However, it was ultimately set aside due to limitations in performance control and architectural flexibility relative to the project's goals.

---

### DirectX

DirectX was avoided primarily due to platform constraints.

Keeping the project independent of a single ecosystem is a priority, and long-term portability is treated as a core requirement rather than an afterthought.

---

## Documentation

<div align="center">

<a href="https://github.com/BiggieBranose/Chronica-Regna-Fractorum/wiki">
  <kbd>View Wiki</kbd>
</a>

</div>

The wiki will gradually expand to include technical breakdowns, development notes, and internal system documentation as the project evolves.

---

## Development Context

This project serves a dual purpose: building a game, and building understanding.

Current areas of focus include:

- low-level graphics programming  
- engine structure and architecture  
- system-level experimentation  

As a result, progress may be uneven, and parts of the project will change or be reworked over time.

---

<div align="right">
  <a href="#chronica-regna-fractorum">
    <kbd>Back to Top</kbd>
  </a>
</div>
