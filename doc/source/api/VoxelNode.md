# VoxelNode

Inherits: [Node3D](https://docs.godotengine.org/en/stable/classes/class_node3d.html)

Inherited by: [VoxelLodTerrain](VoxelLodTerrain.md), [VoxelTerrain](VoxelTerrain.md)

Base class for voxel volumes.

## Properties: 


Type              | Name                           | Default 
----------------- | ------------------------------ | --------
`int`             | [cast_shadow](#i_cast_shadow)  | 1       
`VoxelGenerator`  | [generator](#i_generator)      |         
`int`             | [gi_mode](#i_gi_mode)          | 0       
`VoxelMesher`     | [mesher](#i_mesher)            |         
`VoxelStream`     | [stream](#i_stream)            |         
<p></p>

## Enumerations: 

enum **GIMode**: 

- <span id="i_GI_MODE_DISABLED"></span>**GI_MODE_DISABLED** = **0**
- <span id="i_GI_MODE_BAKED"></span>**GI_MODE_BAKED** = **1**
- <span id="i_GI_MODE_DYNAMIC"></span>**GI_MODE_DYNAMIC** = **2**


## Property Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_cast_shadow"></span> **cast_shadow** = 1


- [VoxelGenerator](VoxelGenerator.md)<span id="i_generator"></span> **generator**

Procedural generator used to load voxel blocks when not present in the stream.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_gi_mode"></span> **gi_mode** = 0


- [VoxelMesher](VoxelMesher.md)<span id="i_mesher"></span> **mesher**

Defines how voxels are transformed into visible meshes.

- [VoxelStream](VoxelStream.md)<span id="i_stream"></span> **stream**

Primary source of persistent voxel data. If left unassigned, the whole volume will use the generator.

_Generated on Nov 11, 2023_
