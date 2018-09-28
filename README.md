Voxel Tools for Godot
=========================

C++ module for creating volumetric worlds in Godot Engine.

![Example screenshot](screenshots/2016_05_04_0319_w800.png)

Setup
------

Install the contents of the repo in a folder under "modules/", named "voxel".

IMPORTANT: if you clone the repo, Git will create the folder as the repo name, "godot_voxel". But because Godot SCons scripts consider the folder name as the module's name, it will generate wrong function calls, so you must rename the folder "voxel".


What this module provides
---------------------------

- Fully editable terrain as long as you call the right functions (see demo: https://github.com/Zylann/voxelgame)
- Voxel storage using 8-bit channels like images for any general purpose
- Data segmentation using blocks of 16x16x16 voxels (so the world can be theoretically infinite)
- Main development oriented for minecraft-style terrain by using voxels as types
- Experimental support for smooth terrain using Transvoxel http://transvoxel.org/, by using voxels as isolevels
- Vertex-based ambient occlusion on voxel edges
- Simple interface for deferred terrain generators (block by block using threads)
- Uses threads to stream terrain as the camera moves
- Optional performance statistics


What this module doesn't provides
-----------------------------------

- Level of detail (not a priority for my project)
- Game specific features such as cave generation or procedural trees (though it might include tools to help doing them)
- Editor tools (only a few things are exposed)
- Import and export of voxel formats
- For reasons why I don't work on X or Y, see https://github.com/Zylann/godot_voxel/issues/17
