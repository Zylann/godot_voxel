Voxel Tools for Godot
=========================

C++ module for creating cube-esque voxel worlds in Godot Engine.

![Example screenshot](screenshots/2016_05_04_0319_w800.png)

Setup
------

Install the contents of the repo in a folder under "modules/", named "voxel".

IMPORTANT: if you clone the repo, Git will create the folder as the repo name, "godot_voxel". But because Godot SCons scripts consider the folder name as the module's name, it will generate wrong function calls, so you must rename the folder "voxel".


Features
---------

- Fully editable terrain (see demo: https://github.com/Zylann/voxelgame)
- Compact voxel storage using 8-bit channels like images
- Data segmentation using blocks of 16x16x16 voxels (so the world can be theoretically infinite)
- Calculates meshes based on grid of voxels. Only visible faces are generated.
- Vertex-based ambient occlusion on voxel edges (comes for free at the cost of slower mesh generation)
- Mesh-based physics based on Godot (high generation cost but works at decent speed)
- Simple interface for deferred terrain generators (block by block)
- Optional profiling information


Ideas TODO
-----------

- Automatic terrain generation (currently not automatic)
- Voxels can be of any shape, not just cubes (not fully accessible yet but present in code)
- VoxelStreamer node to put on players so terrains know which voxels to load around them
- Support internal threading
- Support saving and loading through helper classes
- Promote classes to Node and Resource for better editor experience
- Helpers for structure generation (anything that is bigger than one voxel)
- Convert any imported meshes to Voxel-ready meshes (to comply with a few optimizations)
- Generate Mesh resources from voxels so they can be used in the editor as MeshInstances
- Import resources from other editors like Magicka-voxel
- Volumetric grid algorithms: fluids, pathfinding, lighting...
- Smooth meshes with the Transvoxel algorithm
- Seam-free level of detail (LOD)
- Ability to bake terrains if we don't want them to be editable in game

