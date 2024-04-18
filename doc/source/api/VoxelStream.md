# VoxelStream

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)

Inherited by: [VoxelStreamMemory](VoxelStreamMemory.md), [VoxelStreamRegionFiles](VoxelStreamRegionFiles.md), [VoxelStreamSQLite](VoxelStreamSQLite.md), [VoxelStreamScript](VoxelStreamScript.md)

Implements loading and saving voxel blocks, mainly using files.

## Properties: 


Type                                                                    | Name                                               | Default 
----------------------------------------------------------------------- | -------------------------------------------------- | --------
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)  | [save_generator_output](#i_save_generator_output)  | false   
<p></p>

## Methods: 


Return                                                                        | Signature                                                                                                                                                                                                                                                                   
----------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)          | [emerge_block](#i_emerge_block) ( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod_index )  *(deprecated)* 
[void](#)                                                                     | [flush](#i_flush) ( )                                                                                                                                                                                                                                                       
[Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)  | [get_block_size](#i_get_block_size) ( ) const                                                                                                                                                                                                                               
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)          | [get_used_channels_mask](#i_get_used_channels_mask) ( ) const                                                                                                                                                                                                               
[void](#)                                                                     | [immerge_block](#i_immerge_block) ( [VoxelBuffer](VoxelBuffer.md) buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod_index )  *(deprecated)*   
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)          | [load_voxel_block](#i_load_voxel_block) ( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod_index )       
[void](#)                                                                     | [save_voxel_block](#i_save_voxel_block) ( [VoxelBuffer](VoxelBuffer.md) buffer, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod_index )           
<p></p>

## Enumerations: 

enum **ResultCode**: 

- <span id="i_RESULT_ERROR"></span>**RESULT_ERROR** = **0** --- An error occurred when loading the block. The request will be aborted.
- <span id="i_RESULT_BLOCK_FOUND"></span>**RESULT_BLOCK_FOUND** = **2** --- The block was found.
- <span id="i_RESULT_BLOCK_NOT_FOUND"></span>**RESULT_BLOCK_NOT_FOUND** = **1** --- The block was not found. The requester may fallback on using the generator, if any.


## Property Descriptions

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_save_generator_output"></span> **save_generator_output** = false

When this is enabled, if a block cannot be found in the stream and it gets generated, then the generated block will immediately be saved into the stream. This can be used if the generator is too expensive to run on the fly (like Minecraft does), but it will require more disk usage (amount of I/Os and space) and eventual network traffic. If this setting is off, only modified blocks will be saved.

## Method Descriptions

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_emerge_block"></span> **emerge_block**( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod_index ) 

*This method is deprecated. Use [VoxelStream.load_voxel_block](VoxelStream.md#i_load_voxel_block) instead.*


### [void](#)<span id="i_flush"></span> **flush**( ) 

Forces cached data to be saved to the filesystem. Some streams might use a cache to improve performance of frequent I/Os.

This should not be called frequently if performance is a concern. May be used if you require all data to be written now. Note that implementations should already do this automatically when the resource is destroyed or their configuration changes. Some implementations may do nothing if they have no cache.

Note that terrains save asynchronously, so flushing might not always fulfill your goal if saving tasks are still queued and haven't called into [VoxelStream](VoxelStream.md) yet. See [VoxelTerrain.save_modified_blocks](VoxelTerrain.md#i_save_modified_blocks) or [VoxelLodTerrain.save_modified_blocks](VoxelLodTerrain.md#i_save_modified_blocks).

### [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)<span id="i_get_block_size"></span> **get_block_size**( ) 

*(This method has no documentation)*

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_used_channels_mask"></span> **get_used_channels_mask**( ) 

*(This method has no documentation)*

### [void](#)<span id="i_immerge_block"></span> **immerge_block**( [VoxelBuffer](VoxelBuffer.md) buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod_index ) 

*This method is deprecated. Use [VoxelStream.save_voxel_block](VoxelStream.md#i_save_voxel_block) instead.*


### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_load_voxel_block"></span> **load_voxel_block**( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod_index ) 

*(This method has no documentation)*

### [void](#)<span id="i_save_voxel_block"></span> **save_voxel_block**( [VoxelBuffer](VoxelBuffer.md) buffer, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod_index ) 

`buffer`: Block of voxels to save. It is strongly recommended to not keep a reference to that data afterward, because streams are allowed to cache it, and saved data must represent either snapshots (copies) or last references to the data after the volume they belonged to is destroyed.

_Generated on Apr 06, 2024_
