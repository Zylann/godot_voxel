# VoxelStream

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)


Implements loading and saving voxel blocks, mainly using files.

## Properties: 


Type    | Name                                               | Default 
------- | -------------------------------------------------- | --------
`bool`  | [save_generator_output](#i_save_generator_output)  | false   
<p></p>

## Methods: 


Return                                                                        | Signature                                                                                                                                                                                                                                              
----------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)          | [emerge_block](#i_emerge_block) ( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod )  
[Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)  | [get_block_size](#i_get_block_size) ( ) const                                                                                                                                                                                                          
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)          | [get_used_channels_mask](#i_get_used_channels_mask) ( ) const                                                                                                                                                                                          
[void](#)                                                                     | [immerge_block](#i_immerge_block) ( [VoxelBuffer](VoxelBuffer.md) buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod )    
<p></p>

## Enumerations: 

enum **Result**: 

- **RESULT_ERROR** = **0** --- An error occurred when loading the block. The request will be aborted.
- **RESULT_BLOCK_FOUND** = **2** --- The block was found.
- **RESULT_BLOCK_NOT_FOUND** = **1** --- The block was not found. The requester may fallback on using the generator, if any.


## Property Descriptions

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_save_generator_output"></span> **save_generator_output** = false

When this is enabled, if a block cannot be found in the stream and it gets generated, then the generated block will immediately be saved into the stream. This can be used if the generator is too expensive to run on the fly (like Minecraft does), but it will require more disk usage (amount of I/Os and space) and eventual network traffic. If this setting is off, only modified blocks will be saved.

## Method Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_emerge_block"></span> **emerge_block**( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) 


- [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)<span id="i_get_block_size"></span> **get_block_size**( ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_used_channels_mask"></span> **get_used_channels_mask**( ) 


- [void](#)<span id="i_immerge_block"></span> **immerge_block**( [VoxelBuffer](VoxelBuffer.md) buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) 

`buffer`: Block of voxels to save. It is strongly recommended to not keep a reference to that data afterward, because streams are allowed to cache it, and saved data must represent either snapshots (copies) or last references to the data after the volume they belonged to is destroyed.

_Generated on Nov 06, 2021_
