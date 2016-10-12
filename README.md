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

- Compact voxel storage using 8-bit channels like images
- Calculates meshes based on grid of voxels. Only visible faces are generated.
- Vertex-based ambient occlusion (comes for free at the cost of slower mesh generation)
- Voxels can be of any shape, not just cubes (not fully accessible yet but present in code)


Ideas TODO
-----------

- VoxelMap node for infinite voxel terrain
- VoxelStreamer node to put on players so terrains know which voxels to load around them
- Physics!
- Make voxels editable
- Support internal threading
- Support saving and loading
- Promote classes to Node and Resource for better editor experience
- Interface for terrain generation modules (I don't plan to integrate a noise library in this module, unless it's integrated in Godot core)
- Support structure generation (anything that is bigger than one voxel)
- Convert any imported meshes to Voxel-compatible meshes (to comply with a few optimizations)
- Generate Mesh resources from voxels so they can be used in the editor as MeshInstances
- Import resources from other editors like Magicka-voxel
- Volumetric grid algorithms: fluids, pathfinding, lighting...

