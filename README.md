# Veksel

A audio code playground built with C++, Dear ImGui, and GLFW/OpenGL.

> *Named after **Wechsel** (German for "change")*

<img src="showcase.png" style="max-height:500px"/>

## Requirements

* **C++ Compiler:** Clang, GCC, or MSVC (C++17 or higher)
* **CMake:** Version 3.15 or higher
* **GLFW3 & OpenGL:** Development headers/libraries installed on your system
* **Git:** For fetching submodules

## Getting Started

### 1. Clone the Repository

Make sure to pass `--recursive` so Git initializes and pulls down all submodules (including `imgui`):

```bash
git clone --recursive https://github.com/JoeKenyon/veksel.git
cd veksel
```

### 2. Build and run

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/veksel
```