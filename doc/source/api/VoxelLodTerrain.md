# VoxelLodTerrain

Inherits: [VoxelNode](VoxelNode.md)


Voxel volume using variable level of detail.

## Properties: 


Type           | Name                                                 | Default                                                                                 
-------------- | ---------------------------------------------------- | ----------------------------------------------------------------------------------------
`int`          | [collision_layer](#i_collision_layer)                | 1                                                                                       
`int`          | [collision_lod_count](#i_collision_lod_count)        | 0                                                                                       
`float`        | [collision_margin](#i_collision_margin)              | 0.04                                                                                    
`int`          | [collision_mask](#i_collision_mask)                  | 1                                                                                       
`int`          | [collision_update_delay](#i_collision_update_delay)  | 0                                                                                       
`bool`         | [generate_collisions](#i_generate_collisions)        | true                                                                                    
`int`          | [lod_count](#i_lod_count)                            | 4                                                                                       
`float`        | [lod_distance](#i_lod_distance)                      | 48.0                                                                                    
`float`        | [lod_fade_duration](#i_lod_fade_duration)            | 0.0                                                                                     
`Material`     | [material](#i_material)                              |                                                                                         
`int`          | [mesh_block_size](#i_mesh_block_size)                | 16                                                                                      
`VoxelMesher`  | [mesher](#i_mesher)                                  |                                                                                         
`bool`         | [run_stream_in_editor](#i_run_stream_in_editor)      | true                                                                                    
`int`          | [view_distance](#i_view_distance)                    | 512                                                                                     
`AABB`         | [voxel_bounds](#i_voxel_bounds)                      | AABB( -5.36871e+08, -5.36871e+08, -5.36871e+08, 1.07374e+09, 1.07374e+09, 1.07374e+09 ) 
<p></p>

## Methods: 


Return                                                                              | Signature                                                                                                                                                                                                                                   
----------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [debug_dump_as_scene](#i_debug_dump_as_scene) ( [String](https://docs.godotengine.org/en/stable/classes/class_string.html) path ) const                                                                                                     
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [debug_get_data_block_count](#i_debug_get_data_block_count) ( ) const                                                                                                                                                                       
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)  | [debug_get_data_block_info](#i_debug_get_data_block_info) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) block_pos, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) const        
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [debug_get_mesh_block_count](#i_debug_get_mesh_block_count) ( ) const                                                                                                                                                                       
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)  | [debug_get_mesh_block_info](#i_debug_get_mesh_block_info) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) block_pos, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) const        
[Array](https://docs.godotengine.org/en/stable/classes/class_array.html)            | [debug_get_octrees_detailed](#i_debug_get_octrees_detailed) ( ) const                                                                                                                                                                       
[Array](https://docs.godotengine.org/en/stable/classes/class_array.html)            | [debug_print_sdf_top_down](#i_debug_print_sdf_top_down) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) center, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) extents ) const 
[Array](https://docs.godotengine.org/en/stable/classes/class_array.html)            | [debug_raycast_mesh_block](#i_debug_raycast_mesh_block) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) dir ) const     
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [get_data_block_region_extent](#i_get_data_block_region_extent) ( ) const                                                                                                                                                                   
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [get_data_block_size](#i_get_data_block_size) ( ) const                                                                                                                                                                                     
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [get_process_mode](#i_get_process_mode) ( ) const                                                                                                                                                                                           
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)  | [get_statistics](#i_get_statistics) ( ) const                                                                                                                                                                                               
[VoxelTool](VoxelTool.md)                                                           | [get_voxel_tool](#i_get_voxel_tool) ( )                                                                                                                                                                                                     
[void](#)                                                                           | [save_modified_blocks](#i_save_modified_blocks) ( )                                                                                                                                                                                         
[void](#)                                                                           | [set_process_mode](#i_set_process_mode) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mode )                                                                                                                       
[Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)        | [voxel_to_data_block_position](#i_voxel_to_data_block_position) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) lod_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) arg1 ) const 
[Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)        | [voxel_to_mesh_block_position](#i_voxel_to_mesh_block_position) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) lod_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) arg1 ) const 
<p></p>

## Enumerations: 

enum **ProcessMode**: 

- **PROCESS_MODE_IDLE** = **0**
- **PROCESS_MODE_PHYSICS** = **1**
- **PROCESS_MODE_DISABLED** = **2**


## Property Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_collision_layer"></span> **collision_layer** = 1


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_collision_lod_count"></span> **collision_lod_count** = 0


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_collision_margin"></span> **collision_margin** = 0.04


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_collision_mask"></span> **collision_mask** = 1


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_collision_update_delay"></span> **collision_update_delay** = 0


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_generate_collisions"></span> **generate_collisions** = true


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_lod_count"></span> **lod_count** = 4


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_lod_distance"></span> **lod_distance** = 48.0


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_lod_fade_duration"></span> **lod_fade_duration** = 0.0


- [Material](https://docs.godotengine.org/en/stable/classes/class_material.html)<span id="i_material"></span> **material**


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_mesh_block_size"></span> **mesh_block_size** = 16


- [VoxelMesher](VoxelMesher.md)<span id="i_mesher"></span> **mesher**


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_run_stream_in_editor"></span> **run_stream_in_editor** = true


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_view_distance"></span> **view_distance** = 512


- [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html)<span id="i_voxel_bounds"></span> **voxel_bounds** = AABB( -5.36871e+08, -5.36871e+08, -5.36871e+08, 1.07374e+09, 1.07374e+09, 1.07374e+09 )


## Method Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_debug_dump_as_scene"></span> **debug_dump_as_scene**( [String](https://docs.godotengine.org/en/stable/classes/class_string.html) path ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_debug_get_data_block_count"></span> **debug_get_data_block_count**( ) 


- [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)<span id="i_debug_get_data_block_info"></span> **debug_get_data_block_info**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) block_pos, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_debug_get_mesh_block_count"></span> **debug_get_mesh_block_count**( ) 


- [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)<span id="i_debug_get_mesh_block_info"></span> **debug_get_mesh_block_info**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) block_pos, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) 


- [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_debug_get_octrees_detailed"></span> **debug_get_octrees_detailed**( ) 


- [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_debug_print_sdf_top_down"></span> **debug_print_sdf_top_down**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) center, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) extents ) 


- [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_debug_raycast_mesh_block"></span> **debug_raycast_mesh_block**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) dir ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_data_block_region_extent"></span> **get_data_block_region_extent**( ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_data_block_size"></span> **get_data_block_size**( ) 


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


- [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)<span id="i_voxel_to_data_block_position"></span> **voxel_to_data_block_position**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) lod_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) arg1 ) 


- [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)<span id="i_voxel_to_mesh_block_position"></span> **voxel_to_mesh_block_position**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) lod_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) arg1 ) 


_Generated on Nov 06, 2021_
