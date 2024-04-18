# VoxelNode

Inherits: [Node3D](https://docs.godotengine.org/en/stable/classes/class_node3d.html)

Inherited by: [VoxelLodTerrain](VoxelLodTerrain.md), [VoxelTerrain](VoxelTerrain.md)

Base class for voxel volumes.

## Properties: 


Type                                                                  | Name                                         | Default 
--------------------------------------------------------------------- | -------------------------------------------- | --------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [cast_shadow](#i_cast_shadow)                | 1       
[VoxelGenerator](VoxelGenerator.md)                                   | [generator](#i_generator)                    |         
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [gi_mode](#i_gi_mode)                        | 0       
[VoxelMesher](VoxelMesher.md)                                         | [mesher](#i_mesher)                          |         
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [render_layers_mask](#i_render_layers_mask)  | 1       
[VoxelStream](VoxelStream.md)                                         | [stream](#i_stream)                          |         
<p></p>

## Property Descriptions

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_cast_shadow"></span> **cast_shadow** = 1

*(This property has no documentation)*

### [VoxelGenerator](VoxelGenerator.md)<span id="i_generator"></span> **generator**

Procedural generator used to load voxel blocks when not present in the stream.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_gi_mode"></span> **gi_mode** = 0

*(This property has no documentation)*

### [VoxelMesher](VoxelMesher.md)<span id="i_mesher"></span> **mesher**

Defines how voxels are transformed into visible meshes.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_render_layers_mask"></span> **render_layers_mask** = 1

*(This property has no documentation)*

### [VoxelStream](VoxelStream.md)<span id="i_stream"></span> **stream**

Primary source of persistent voxel data. If left unassigned, the whole volume will use the generator.

_Generated on Apr 06, 2024_
