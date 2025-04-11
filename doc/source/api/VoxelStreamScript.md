# VoxelStreamScript

Inherits: [VoxelStream](VoxelStream.md)

Base class for custom streams defined with a script.

## Methods: 


Return                                                                | Signature                                                                                                                                                                                                                                                                   
--------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [_get_used_channels_mask](#i__get_used_channels_mask) ( ) virtual const                                                                                                                                                                                                     
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [_load_voxel_block](#i__load_voxel_block) ( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) position_in_blocks, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) virtual 
[void](#)                                                             | [_save_voxel_block](#i__save_voxel_block) ( [VoxelBuffer](VoxelBuffer.md) buffer, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) position_in_blocks, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) virtual     
<p></p>

## Method Descriptions

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i__get_used_channels_mask"></span> **_get_used_channels_mask**( ) 

Tells which channels in [VoxelBuffer](VoxelBuffer.md) are supported to save voxel data, in case the stream only saves specific ones.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i__load_voxel_block"></span> **_load_voxel_block**( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) position_in_blocks, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) 

Called when a block of voxels needs to be loaded. Assumes `out_buffer` always has the same size. Returns [VoxelStream.ResultCode](VoxelStream.md#enumerations).

### [void](#)<span id="i__save_voxel_block"></span> **_save_voxel_block**( [VoxelBuffer](VoxelBuffer.md) buffer, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) position_in_blocks, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) 

Called when a block of voxels needs to be saved. Assumes `out_buffer` always has the same size.

_Generated on Mar 23, 2025_
