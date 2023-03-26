# VoxelNode

Inherits: [Node3D](https://docs.godotengine.org/en/stable/classes/class_node3d.html)


Base class for voxel volumes.

## Properties: 


Type              | Name                       | Default 
----------------- | -------------------------- | --------
`VoxelGenerator`  | [generator](#i_generator)  |         
`int`             | [gi_mode](#i_gi_mode)      | 0       
`VoxelMesher`     | [mesher](#i_mesher)        |         
`VoxelStream`     | [stream](#i_stream)        |         
<p></p>

## Enumerations: 

enum **GIMode**: 

- **GI_MODE_DISABLED** = **0**
- **GI_MODE_BAKED** = **1**
- **GI_MODE_DYNAMIC** = **2**


## Property Descriptions

- [VoxelGenerator](VoxelGenerator.md)<span id="i_generator"></span> **generator**

Procedural generator used to load voxel blocks when not present in the stream.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_gi_mode"></span> **gi_mode** = 0


- [VoxelMesher](VoxelMesher.md)<span id="i_mesher"></span> **mesher**

Defines how voxels are transformed into visible meshes.

- [VoxelStream](VoxelStream.md)<span id="i_stream"></span> **stream**

Primary source of persistent voxel data. If left unassigned, the whole volume will use the generator.

_Generated on Mar 26, 2023_
