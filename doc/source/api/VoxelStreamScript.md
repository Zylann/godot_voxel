# VoxelStreamScript

Inherits: [VoxelStream](VoxelStream.md)


Base class for custom streams defined with a script.

## Methods: 


Return                                                                | Signature                                                                                                                                                                                                                                                                 
--------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [_get_used_channels_mask](#i__get_used_channels_mask) ( ) virtual const                                                                                                                                                                                                   
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [_load_voxel_block](#i__load_voxel_block) ( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) virtual 
[void](#)                                                             | [_save_voxel_block](#i__save_voxel_block) ( [VoxelBuffer](VoxelBuffer.md) buffer, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) virtual     
<p></p>

## Method Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i__get_used_channels_mask"></span> **_get_used_channels_mask**( ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i__load_voxel_block"></span> **_load_voxel_block**( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) 


- [void](#)<span id="i__save_voxel_block"></span> **_save_voxel_block**( [VoxelBuffer](VoxelBuffer.md) buffer, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) 


_Generated on Mar 26, 2023_
