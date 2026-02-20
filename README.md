# Horizon Unseen

A 2D side-scrolling shooter in the vein of R-Type, built with Vulkan and ImGui.

## Prerequisites

- CMake 3.20 or higher
- Vulkan SDK (installed and VULKAN_SDK environment variable set)
- C++17 compatible compiler (MSVC, GCC, or Clang)

## Dependencies

The project uses the following third-party libraries:
- **GLFW** - Window and input management
- **ImGui** - UI rendering
- **Vulkan** - Graphics API

## Setup

1. **Clone dependencies** into the `external/` folder:
```bash
cd external
git clone https://github.com/glfw/glfw.git
git clone https://github.com/ocornut/imgui.git
cd ..
```

2. **Generate build files**:
```bash
mkdir build
cd build
cmake ..
```

3. **Build the project**:
```bash
cmake --build . --config Release
```

Or open the generated Visual Studio solution in `build/HorizonUnseen.sln`

## Project Structure

```
HorizonUnseen/
??? CMakeLists.txt           # Root CMake configuration
??? external/                # Third-party dependencies
?   ??? CMakeLists.txt
?   ??? glfw/               # (clone here)
?   ??? imgui/              # (clone here)
??? src/
?   ??? main.cpp            # Entry point
?   ??? Application.h/cpp   # Main application class
?   ??? Renderer/           # Vulkan rendering
?   ?   ??? VulkanRenderer.h/cpp
?   ?   ??? VulkanContext.h/cpp
?   ??? Game/               # Game logic
?   ?   ??? Entity.h/cpp
?   ?   ??? Player.h/cpp
?   ?   ??? GameScene.h/cpp
?   ??? Systems/            # Game systems
?       ??? InputSystem.h/cpp
??? shaders/                # GLSL shaders
    ??? sprite.vert
    ??? sprite.frag
```

## Controls

- **WASD / Arrow Keys** - Move player
- **Space** - Shoot (TODO)
- **ESC** - Exit game

## Roadmap

### Phase 1: Foundation (Current)
- [x] CMake project setup
- [x] Window creation with GLFW
- [x] Basic Vulkan initialization
- [x] Player entity with movement
- [x] Input system

### Phase 2: Core Rendering
- [ ] Complete Vulkan setup (swapchain, render pass, pipeline)
- [ ] Sprite rendering system
- [ ] Texture loading
- [ ] Camera/viewport system
- [ ] ImGui integration

### Phase 3: Gameplay
- [ ] Shooting mechanics
- [ ] Enemy types
- [ ] Collision detection
- [ ] Wave spawning system
- [ ] Power-ups

### Phase 4: Polish
- [ ] Particle effects
- [ ] Sound effects and music
- [ ] Background parallax scrolling
- [ ] UI/HUD
- [ ] Level progression

## License

MIT License
