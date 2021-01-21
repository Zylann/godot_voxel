# VoxelStream

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)




## Properties: 


Type    | Name                                               | Default 
------- | -------------------------------------------------- | --------
`bool`  | [save_generator_output](#i_save_generator_output)  | true    
<p></p>

## Methods: 


Return                                                                        | Signature                                                                                                                                                                                                                                              
----------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                                     | [emerge_block](#i_emerge_block) ( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod )  
[Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)  | [get_block_size](#i_get_block_size) ( ) const                                                                                                                                                                                                          
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)          | [get_used_channels_mask](#i_get_used_channels_mask) ( ) const                                                                                                                                                                                          
[void](#)                                                                     | [immerge_block](#i_immerge_block) ( [VoxelBuffer](VoxelBuffer.md) buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod )    
<p></p>

## Enumerations: 

enum **Result**: 

- **RESULT_ERROR** = **0**
- **RESULT_BLOCK_FOUND** = **2**
- **RESULT_BLOCK_NOT_FOUND** = **1**


## Property Descriptions

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_save_generator_output"></span> **save_generator_output** = true


## Method Descriptions

- [void](#)<span id="i_emerge_block"></span> **emerge_block**( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) 



- [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)<span id="i_get_block_size"></span> **get_block_size**( ) 



- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_used_channels_mask"></span> **get_used_channels_mask**( ) 



- [void](#)<span id="i_immerge_block"></span> **immerge_block**( [VoxelBuffer](VoxelBuffer.md) buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) 



_Generated on Jan 20, 2021_
