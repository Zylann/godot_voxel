Voxel Tools for Godot
=========================

A C++ module for creating volumetric worlds in Godot Engine.

<img src="doc/images/blocky_screenshot.png" width="800" />
<img src="doc/images/smooth_screenshot.png" width="800" />
<img src="doc/images/textured-terrain.jpg" width="800" />

Features
---------------------------

- Realtime editable, 3D based terrain (Unlike a heightmap based terrain, this allows for overhangs, tunnels, and user creation/destruction)
- Physics based collision and raycast support
- Infinite terrains made by paging sections in and out
- Voxel data is streamed from a variety of sources, which includes the ability to write your own generators
- Minecraft-style blocky voxel terrain, with multiple materials and baked ambient occlusion
- Smooth terrain using Transvoxel
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

### Prebuilt binaries

Ready-to-use versions of Godot including the module exist, though they might not be 100% up to date.

- Tokisan Games
    - [Tokisan Games binaries](http://tokisan.com/godot-binaries)

- Zylann
    - Godot 3.2.2 editor for Windows 64 bits (TBD)


### Compiling

Compiling the source yourself is the best way to get the very latest version.
Please see the [Getting Started Guide](doc/01_get-started.md) for instructions.


### Examples

- Documentation: [Getting Started Guide](doc/01_get-started.md)
- Zylann's demos: [Zylann's demos](https://github.com/Zylann/voxelgame)
- TinmanJuggernaut's demos: [TinmanJuggernaut's demo](https://github.com/tinmanjuggernaut/voxelgame)


Roadmap
---------

These are some ideas that may or may not be implemented in the future:

* Texturing on smooth terrain
* Editor preview and authoring
* Improving LOD performance
* Other meshing algorithms (e.g. dual contouring)
* GPU offloading (Maybe when Godot 4+ supports compute shaders)
