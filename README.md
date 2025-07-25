Voxel Tools for Godot
=========================

A C++ module/extension for creating volumetric terrains in Godot Engine 4.

[![🚪 Windows Builds](https://github.com/Zylann/godot_voxel/actions/workflows/windows.yml/badge.svg)](https://github.com/Zylann/godot_voxel/actions/workflows/windows.yml)
[![🐧 Linux Builds](https://github.com/Zylann/godot_voxel/actions/workflows/linux.yml/badge.svg)](https://github.com/Zylann/godot_voxel/actions/workflows/linux.yml)
[![🐒 Mono Builds](https://github.com/Zylann/godot_voxel/actions/workflows/mono.yml/badge.svg)](https://github.com/Zylann/godot_voxel/actions/workflows/mono.yml)
[![🧩 GDExtension Builds](https://github.com/Zylann/godot_voxel/actions/workflows/extension_ci.yml/badge.svg)](https://github.com/Zylann/godot_voxel/actions/workflows/extension_ci.yml)
[![Documentation Status](https://readthedocs.org/projects/voxel-tools/badge/?version=latest)](https://voxel-tools.readthedocs.io/en/latest/?badge=latest)

[![Discord](https://img.shields.io/discord/850070170793410582?style=flat-square&logo=discord "Discord")](https://discord.gg/pkXmESmrAR)

![Blocky screenshot](doc/source/images/blocky_screenshot.webp)
![Smooth screenshot](doc/source/images/smooth_screenshot.webp)
![Textured screenshot](doc/source/images/textured-terrain.jpg)

Features
---------------------------

- Realtime 3D terrain editable in-game (Unlike a heightmap based terrain, this allows for overhangs, tunnels, and user creation/destruction)
- Polygon-based: voxels are transformed into chunked meshes to be rendered
- Godot physics integration + alternate fast Minecraft-like collisions
- Infinite terrains made by paging chunks in and out
- Voxel data is streamed from a variety of sources, which includes the ability to write your own generators
- Minecraft-style blocky voxel terrain, with multiple materials and baked ambient occlusion
- Smooth terrain with level of detail using Transvoxel
- Voxel storage using 8-bit or 16-bit channels for any general purpose
- Instancing system to spawn foliage, rocks and other decoration on surfaces

Check the [changelog](https://voxel-tools.readthedocs.io/en/latest/changelog/) for more recent details.


Documentation
---------------

- [Main documentation](https://voxel-tools.readthedocs.io/en/latest/)
- [How to install](https://voxel-tools.readthedocs.io/en/latest/getting_the_module/)
- [Quick start](https://voxel-tools.readthedocs.io/en/latest/quick_start/)


Roadmap
---------

Check [Feature Branches](https://github.com/Zylann/godot_voxel/issues/640) to see work-in-progress.

Some areas of interest:

* Multiplayer synchronization
* Smooth voxel texturing
* Level of detail with blocky voxels
* Make GDExtension work


Supporters
-----------

This module is a non-profit project developed by voluntary contributors. The following is the list of who donated at least once.
Thanks for your support :)

### Gold supporters

```
Aaron Franke (aaronfranke)
Bewildering
Eerrikki
```

### Silver supporters

```
TheConceptBoy
Chris Bolton (yochrisbolton)
Gamerfiend (Snowminx) 
greenlion (Justin Swanhart) 
segfault-god (jp.owo.Manda)
RonanZe
Phyronnaz
NoFr1ends (Lynx)
Kluskey (Jared McCluskey)
Trey2k (Trey Moller)
marcinn (Marcin Nowak)
bfoster68
gumby-cmyk
Joshua Woods (jpw1991)
jjoshpoland (Josh)
jbbieber1127 (John Bieber)
```

### Supporters

```
rcorre (Ryan Roden-Corrent) 
duchainer (Raphaël Duchaîne)
MadMartian
stackdump (stackdump.eth)
Treer
MrGreaterThan
lenis0012
ilievmark (Iliev Mark)
OrbitalHare
matthewhilton (Matthew Hilton)
Pugulishus
Fabian (nan0m)
SummitCollie
nulshift
ddel-rio (Daniel del Río Román)
Cyberphinx
Mia (Tigxette)
geryan (OGeryan)
```


