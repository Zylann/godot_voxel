Voxel Tools for Godot
=========================

A C++ module for creating volumetric worlds in Godot Engine.

![Blocky screenshot](doc/source/images/blocky_screenshot.png)
![Smooth screenshot](doc/source/images/smooth_screenshot.png)
![Textured screenshot](doc/source/images/textured-terrain.jpg)

Features
---------------------------

- Realtime editable, 3D based terrain (Unlike a heightmap based terrain, this allows for overhangs, tunnels, and user creation/destruction)
- Physics based collision and raycast support
- Infinite terrains made by paging sections in and out
- Voxel data is streamed from a variety of sources, which includes the ability to write your own generators
- Minecraft-style blocky voxel terrain, with multiple materials and baked ambient occlusion
- Smooth terrain using Transvoxel
- Levels of detail for smooth terrain
- Voxel storage using 8-bit or 16-bit channels for any general purpose


Documentation
---------------

- [Main documentation](https://voxel-tools.readthedocs.io/en/latest/)
- [How to get the module](https://voxel-tools.readthedocs.io/en/latest/getting_the_module/)
- [Quick start](https://voxel-tools.readthedocs.io/en/latest/quick_start/)


Roadmap
---------

These are some ideas that may or may not be implemented in the future:

* Instancing for foliage and rocks
* Texturing on smooth terrain
* Editor preview and authoring
* Improving LOD performance
* Other meshing algorithms (e.g. dual contouring)
* GPU offloading (Maybe when Godot 4+ supports compute shaders)


Supporters
-----------

This module is a non-profit project developped by voluntary contributors. The following is the list of the current donors.
Thanks for your support :)

### Supporters

```
wacyym
Sergey Lapin (slapin)
Jonas (NoFr1ends)
lenis0012
Phyronnaz
RonanZe
furtherorbit
jp.owo.Manda (segfault-god)
```

