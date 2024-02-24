# VoxelNode

Inherits: [Node3D](https://docs.godotengine.org/en/stable/classes/class_node3d.html)

Inherited by: [VoxelLodTerrain](VoxelLodTerrain.md), [VoxelTerrain](VoxelTerrain.md)

Base class for voxel volumes.

## Properties: 


Type              | Name                                         | Default 
----------------- | -------------------------------------------- | --------
`int`             | [cast_shadow](#i_cast_shadow)                | 1       
`VoxelGenerator`  | [generator](#i_generator)                    |         
`int`             | [gi_mode](#i_gi_mode)                        | 0       
`VoxelMesher`     | [mesher](#i_mesher)                          |         
`int`             | [render_layers_mask](#i_render_layers_mask)  | 1       
`VoxelStream`     | [stream](#i_stream)                          |         
<p></p>

## Property Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_cast_shadow"></span> **cast_shadow** = 1


- [VoxelGenerator](VoxelGenerator.md)<span id="i_generator"></span> **generator**

Procedural generator used to load voxel blocks when not present in the stream.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_gi_mode"></span> **gi_mode** = 0


- [VoxelMesher](VoxelMesher.md)<span id="i_mesher"></span> **mesher**

Defines how voxels are transformed into visible meshes.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_render_layers_mask"></span> **render_layers_mask** = 1


- [VoxelStream](VoxelStream.md)<span id="i_stream"></span> **stream**

Primary source of persistent voxel data. If left unassigned, the whole volume will use the generator.

_Generated on Feb 24, 2024_
