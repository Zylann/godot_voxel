Voxel Tools for Godot
=========================

C++ module for creating cube-esque voxel worlds in Godot Engine.

![Example screenshot](screenshots/2016_05_04_0319_w800.png)

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

