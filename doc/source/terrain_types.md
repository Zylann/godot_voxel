Terrain types
=================

The module provides two types of voxel terrains, each with their own pros and cons. They have a lot in common, but also handle blocks in a very different way.


Common properties
-------------------

### Voxel properties

Each voxel node inherits `VoxelNode`, which defines properties they all have in common.

- `stream`: a `VoxelStream` resource, which allows to load and save voxels. See [Voxel Streams](streams.md)
- `generator`: a `VoxelGenerator` resource, which allows to populate the volume with generated voxels. See [Generators](generators.md)
- `mesher`: a `VoxelMesher` resource, which defines how the voxels will look like. It also defines collision meshes if enabled.


### Collisions

Physics based collisions are enabled by default, and behave like a static body. It provides both raycasting and collision detection. You can turn it on or off by setting the `generate_collisions` option on any of the terrain nodes. Or you can enable or disable it in code.

The collision is built along with the mesh. So any blocks that have already been built will not be affected by this setting unless they are regenerated.

You can also turn on the collision wire mesh for debugging. In the editor, look under the Debug menu for `Visible Collision Shapes`.

![Collision shapes](images/debug-collision-shapes.gif)


VoxelTerrain
--------------

This node creates blocks in a cubic grid around the `VoxelViewer`. It does not handle level of detail, so it has a limited view distance. This also means it's well suited for smaller terrains or voxel volumes. It can generally handle around 300 voxels in each direction before it gets slow.

TODO More info



VoxelLodTerrain
----------------

This node creates blocks in an octree around the viewer, such that closest blocks are smaller and have a higher level of detail, and blocks far away are much bigger with lower level of detail. This allows to have a much larger view distance while using fewer resources.

Blocky meshers currently don't support it. Only smooth meshers work.

TODO More info

