Voxel Tools for Godot
=========================

A C++ module for creating volumetric worlds in Godot Engine.

<img src="doc/images/blocky_screenshot.png" width="800" />
<img src="doc/images/smooth_screenshot.png" width="800" />
<img src="doc/images/textured-terrain.jpg" width="800" />

Features
---------------------------

- Realtime editable, 3D based terrain (Unlike a heightmap based terrain, this allows for overhangs, tunnels, and user creation/destruction)
- Full collision support
- Infinite terrains made by paging sections in and out
- Voxel data is streamed from a variety of sources, which includes the ability to write your own
- Minecraft-style blocky voxel terrain, with multiple materials and baked ambient occlusion
- Smooth terrain using Dual Marching Cubes
- Levels of detail for smooth terrain
- Voxel storage using 8-bit channels for any general purpose


What This Module Doesn't Provide
-----------------------------------

- Levels of detail for blocky terrain
- Game specific features such as cave generation or procedural trees (though it might include tools to help doing them)
- Editor tools (only a few things are exposed)
- Import and export of voxel formats


How To Install And Use
-------------------------

Voxel Tools is a custom C++ module for Godot 3.1+. It must be compiled into the engine to work. 

Please see the [Getting Started Guide](doc/01_get-started.md) for instructions, or [Zylann's demos](https://github.com/Zylann/voxelgame) and [TinmanJuggernaut's demo](https://github.com/tinmanjuggernaut/voxelgame) for working examples.


Roadmap
---------

These are some ideas that may or may not be implemented in the future:

* LOD (in development)
* Support general voxel use (not just terrains)
* Transvoxel and other meshing algorithms
* GPU Offloading (Maybe when Godot 4+ supports compute shaders)
