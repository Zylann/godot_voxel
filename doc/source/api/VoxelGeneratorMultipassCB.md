# VoxelGeneratorMultipassCB

Inherits: [VoxelGenerator](VoxelGenerator.md)

!!! warning
    This class is marked as experimental. It is subject to likely change or possible removal in future versions. Use at your own discretion.
Scriptable generator working on columns of blocks and multiple passes.

## Description: 

This generator can be implemented with a script to generate terrain in columns of blocks instead of only block by block.

It allows to use multiple passes, where every pass can access results of the previous ones, and allowing access to neighbor columns.

The height of columns is not infinite, but it is possible to define what generates above and below, using a single-pass per-block fallback.

It may only be used with [VoxelTerrain](VoxelTerrain.md).

## Properties: 


Type   | Name                                             | Default 
------ | ------------------------------------------------ | --------
`int`  | [column_base_y_blocks](#i_column_base_y_blocks)  | -4      
`int`  | [column_height_blocks](#i_column_height_blocks)  | 8       
`int`  | [pass_count](#i_pass_count)                      | 1       
<p></p>

## Methods: 


Return                                                                                    | Signature                                                                                                                                                                                                             
----------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                                                 | [_generate_block_fallback](#i__generate_block_fallback) ( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) origin_in_voxels ) virtual         
[void](#)                                                                                 | [_generate_pass](#i__generate_pass) ( [VoxelToolMultipassGenerator](VoxelToolMultipassGenerator.md) voxel_tool, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) pass_index ) virtual             
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                      | [_get_used_channels_mask](#i__get_used_channels_mask) ( ) virtual const                                                                                                                                               
[VoxelBuffer[]](https://docs.godotengine.org/en/stable/classes/class_voxelbuffer[].html)  | [debug_generate_test_column](#i_debug_generate_test_column) ( [Vector2i](https://docs.godotengine.org/en/stable/classes/class_vector2i.html) column_position_blocks )                                                 
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                      | [get_pass_extent_blocks](#i_get_pass_extent_blocks) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) pass_index ) const                                                                         
[void](#)                                                                                 | [set_pass_extent_blocks](#i_set_pass_extent_blocks) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) pass_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) extent )  
<p></p>

## Constants: 

- <span id="i_MAX_PASSES"></span>**MAX_PASSES** = **4**
- <span id="i_MAX_PASS_EXTENT"></span>**MAX_PASS_EXTENT** = **2**

## Property Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_column_base_y_blocks"></span> **column_base_y_blocks** = -4

Lowest altitude of columns, in blocks.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_column_height_blocks"></span> **column_height_blocks** = 8

Height of columns, in blocks.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_pass_count"></span> **pass_count** = 1

Number of passes columns will go through before being considered fully generated. More passes increases memory and processing cost.

## Method Descriptions

- [void](#)<span id="i__generate_block_fallback"></span> **_generate_block_fallback**( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) origin_in_voxels ) 

Called for every block to generate above or below the column-based region. For example you can decide to generate air above, and bedrock below.

- [void](#)<span id="i__generate_pass"></span> **_generate_pass**( [VoxelToolMultipassGenerator](VoxelToolMultipassGenerator.md) voxel_tool, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) pass_index ) 

Called once per pass for every column of blocks.

The passed `voxel_tool` must be used to get information about the area to generate, and fill/edit this area with voxels. Important: do not keep this object in a member variable for later re-use. You can only use it in the current call to this method.

You may use `pass_index` to do something different in each pass. For example, 0 could be base ground with Perlin noise, 1 could plant trees and other structures.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i__get_used_channels_mask"></span> **_get_used_channels_mask**( ) 

Use this to indicate which channels your generator will use. It returns a bitmask, so for example you may provide information like this: `(1 << channel1) | (1 << channel2)`

- [VoxelBuffer[]](https://docs.godotengine.org/en/stable/classes/class_voxelbuffer[].html)<span id="i_debug_generate_test_column"></span> **debug_generate_test_column**( [Vector2i](https://docs.godotengine.org/en/stable/classes/class_vector2i.html) column_position_blocks ) 

Testing method that will fully generate all blocks of a specific column, and returns them.

This function doesn't use any threads and doesn't use the internal cache, so it will be very slow. However, it allows to test or debug your script more easily, using an isolated scene for example.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_pass_extent_blocks"></span> **get_pass_extent_blocks**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) pass_index ) 

Gets how many blocks a pass can access around it (note: a block is 16x16x16 voxels by default).

- [void](#)<span id="i_set_pass_extent_blocks"></span> **set_pass_extent_blocks**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) pass_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) extent ) 

Sets how many blocks a pass can access around columns when they generate (note: a block is 16x16x16 voxels by default).

By design, the first pass is not allowed to access neighbors, so it will remain 0.

Following passes are designed to access at least 1 block away. Such passes don't support 0 because it would be the same as simply putting your logic in a previous pass.

Increasing extent will also increase the cost of the generator, both in memory and processing time, so it should be balanced with caution.

_Generated on Feb 24, 2024_
