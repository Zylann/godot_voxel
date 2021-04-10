# VoxelLodTerrain

Inherits: [VoxelNode](VoxelNode.md)


Voxel volume using variable level of detail.

## Properties: 


Type           | Name                                             | Default                                                                                 
-------------- | ------------------------------------------------ | ----------------------------------------------------------------------------------------
`int`          | [collision_lod_count](#i_collision_lod_count)    | -1                                                                                      
`bool`         | [generate_collisions](#i_generate_collisions)    | true                                                                                    
`int`          | [lod_count](#i_lod_count)                        | 4                                                                                       
`float`        | [lod_split_scale](#i_lod_split_scale)            | 3.0                                                                                     
`Material`     | [material](#i_material)                          |                                                                                         
`VoxelMesher`  | [mesher](#i_mesher)                              |                                                                                         
`bool`         | [run_stream_in_editor](#i_run_stream_in_editor)  | true                                                                                    
`int`          | [view_distance](#i_view_distance)                | 512                                                                                     
`AABB`         | [voxel_bounds](#i_voxel_bounds)                  | AABB( -5.36871e+08, -5.36871e+08, -5.36871e+08, 1.07374e+09, 1.07374e+09, 1.07374e+09 ) 
<p></p>

## Methods: 


Return                                                                              | Signature                                                                                                                                                                                                                                   
----------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [debug_dump_as_scene](#i_debug_dump_as_scene) ( [String](https://docs.godotengine.org/en/stable/classes/class_string.html) path ) const                                                                                                     
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [debug_get_block_count](#i_debug_get_block_count) ( ) const                                                                                                                                                                                 
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)  | [debug_get_block_info](#i_debug_get_block_info) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) block_pos, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) const                  
[Array](https://docs.godotengine.org/en/stable/classes/class_array.html)            | [debug_get_octrees](#i_debug_get_octrees) ( ) const                                                                                                                                                                                         
[Array](https://docs.godotengine.org/en/stable/classes/class_array.html)            | [debug_print_sdf_top_down](#i_debug_print_sdf_top_down) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) center, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) extents ) const 
[Array](https://docs.godotengine.org/en/stable/classes/class_array.html)            | [debug_raycast_block](#i_debug_raycast_block) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) dir ) const               
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [get_block_region_extent](#i_get_block_region_extent) ( ) const                                                                                                                                                                             
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [get_block_size](#i_get_block_size) ( ) const                                                                                                                                                                                               
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [get_process_mode](#i_get_process_mode) ( ) const                                                                                                                                                                                           
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)  | [get_statistics](#i_get_statistics) ( ) const                                                                                                                                                                                               
[VoxelTool](VoxelTool.md)                                                           | [get_voxel_tool](#i_get_voxel_tool) ( )                                                                                                                                                                                                     
[void](#)                                                                           | [save_modified_blocks](#i_save_modified_blocks) ( )                                                                                                                                                                                         
[void](#)                                                                           | [set_process_mode](#i_set_process_mode) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mode )                                                                                                                       
[Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)        | [voxel_to_block_position](#i_voxel_to_block_position) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) lod_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) arg1 ) const           
<p></p>

## Enumerations: 

enum **ProcessMode**: 

- **PROCESS_MODE_IDLE** = **0**
- **PROCESS_MODE_PHYSICS** = **1**
- **PROCESS_MODE_DISABLED** = **2**


## Property Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_collision_lod_count"></span> **collision_lod_count** = -1


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_generate_collisions"></span> **generate_collisions** = true


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_lod_count"></span> **lod_count** = 4


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_lod_split_scale"></span> **lod_split_scale** = 3.0


- [Material](https://docs.godotengine.org/en/stable/classes/class_material.html)<span id="i_material"></span> **material**


- [VoxelMesher](VoxelMesher.md)<span id="i_mesher"></span> **mesher**


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_run_stream_in_editor"></span> **run_stream_in_editor** = true


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_view_distance"></span> **view_distance** = 512


- [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html)<span id="i_voxel_bounds"></span> **voxel_bounds** = AABB( -5.36871e+08, -5.36871e+08, -5.36871e+08, 1.07374e+09, 1.07374e+09, 1.07374e+09 )


## Method Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_debug_dump_as_scene"></span> **debug_dump_as_scene**( [String](https://docs.godotengine.org/en/stable/classes/class_string.html) path ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_debug_get_block_count"></span> **debug_get_block_count**( ) 


- [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)<span id="i_debug_get_block_info"></span> **debug_get_block_info**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) block_pos, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) 


- [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_debug_get_octrees"></span> **debug_get_octrees**( ) 


- [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_debug_print_sdf_top_down"></span> **debug_print_sdf_top_down**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) center, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) extents ) 


- [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_debug_raycast_block"></span> **debug_raycast_block**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) dir ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_block_region_extent"></span> **get_block_region_extent**( ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_block_size"></span> **get_block_size**( ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_process_mode"></span> **get_process_mode**( ) 


- [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)<span id="i_get_statistics"></span> **get_statistics**( ) 

Gets debug information about how much time is spent processing the terrain.

The returned dictionary has the following structure:

```gdscript
{
	"time_detect_required_blocks": int,
	"time_request_blocks_to_load": int,
	"time_process_load_responses": int,
	"time_request_blocks_to_update": int,
	"time_process_update_responses": int,
	"remaining_main_thread_blocks": int,
	"dropped_block_loads": int,
	"dropped_block_meshs": int,
	"updated_blocks": int,
	"blocked_lods": int
}

```

- [VoxelTool](VoxelTool.md)<span id="i_get_voxel_tool"></span> **get_voxel_tool**( ) 


- [void](#)<span id="i_save_modified_blocks"></span> **save_modified_blocks**( ) 


- [void](#)<span id="i_set_process_mode"></span> **set_process_mode**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mode ) 


- [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)<span id="i_voxel_to_block_position"></span> **voxel_to_block_position**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) lod_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) arg1 ) 


_Generated on Apr 10, 2021_
