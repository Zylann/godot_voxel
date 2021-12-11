# VoxelStreamScript

Inherits: [VoxelStream](VoxelStream.md)


Base class for custom streams defined with a script.

## Methods: 


Return                                                                | Signature                                                                                                                                                                                                                                                       
--------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                             | [_emerge_block](#i__emerge_block) ( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) virtual 
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [_get_used_channels_mask](#i__get_used_channels_mask) ( ) virtual                                                                                                                                                                                               
[void](#)                                                             | [_immerge_block](#i__immerge_block) ( [VoxelBuffer](VoxelBuffer.md) buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) virtual   
<p></p>

## Method Descriptions

- [void](#)<span id="i__emerge_block"></span> **_emerge_block**( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) 

`out_buffer`: Buffer in which to populate voxel data. It will never be `null` and will have the requested size. It is only valid for this function, do not store it anywhere after the end.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i__get_used_channels_mask"></span> **_get_used_channels_mask**( ) 


- [void](#)<span id="i__immerge_block"></span> **_immerge_block**( [VoxelBuffer](VoxelBuffer.md) buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) 

`buffer`: Buffer of voxel data to save. It is allowed to keep a reference to it for caching purposes, as saved data will either be snapshots or only references left after removal of a volume.

_Generated on Nov 06, 2021_
