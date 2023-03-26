# VoxelLodTerrain

Inherits: [VoxelNode](VoxelNode.md)


Voxel volume using variable level of detail.

## Description: 

Renders large terrain using variable level of details. This is preferably used with smooth meshing such as [VoxelMesherTransvoxel](VoxelMesherTransvoxel.md).

## Properties: 


Type        | Name                                                                               | Default                                                                               
----------- | ---------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------
`int`       | [collision_layer](#i_collision_layer)                                              | 1                                                                                     
`int`       | [collision_lod_count](#i_collision_lod_count)                                      | 0                                                                                     
`float`     | [collision_margin](#i_collision_margin)                                            | 0.04                                                                                  
`int`       | [collision_mask](#i_collision_mask)                                                | 1                                                                                     
`int`       | [collision_update_delay](#i_collision_update_delay)                                | 0                                                                                     
`bool`      | [full_load_mode_enabled](#i_full_load_mode_enabled)                                | false                                                                                 
`bool`      | [generate_collisions](#i_generate_collisions)                                      | true                                                                                  
`int`       | [lod_count](#i_lod_count)                                                          | 4                                                                                     
`float`     | [lod_distance](#i_lod_distance)                                                    | 48.0                                                                                  
`float`     | [lod_fade_duration](#i_lod_fade_duration)                                          | 0.0                                                                                   
`Material`  | [material](#i_material)                                                            |                                                                                       
`int`       | [mesh_block_size](#i_mesh_block_size)                                              | 16                                                                                    
`int`       | [normalmap_begin_lod_index](#i_normalmap_begin_lod_index)                          | 2                                                                                     
`bool`      | [normalmap_enabled](#i_normalmap_enabled)                                          | false                                                                                 
`int`       | [normalmap_max_deviation_degrees](#i_normalmap_max_deviation_degrees)              | 60                                                                                    
`bool`      | [normalmap_octahedral_encoding_enabled](#i_normalmap_octahedral_encoding_enabled)  | false                                                                                 
`int`       | [normalmap_tile_resolution_max](#i_normalmap_tile_resolution_max)                  | 8                                                                                     
`int`       | [normalmap_tile_resolution_min](#i_normalmap_tile_resolution_min)                  | 4                                                                                     
`bool`      | [normalmap_use_gpu](#i_normalmap_use_gpu)                                          | false                                                                                 
`bool`      | [run_stream_in_editor](#i_run_stream_in_editor)                                    | true                                                                                  
`bool`      | [threaded_update_enabled](#i_threaded_update_enabled)                              | false                                                                                 
`int`       | [view_distance](#i_view_distance)                                                  | 512                                                                                   
`AABB`      | [voxel_bounds](#i_voxel_bounds)                                                    | AABB(-5.36871e+08, -5.36871e+08, -5.36871e+08, 1.07374e+09, 1.07374e+09, 1.07374e+09) 
<p></p>

## Methods: 


Return                                                                              | Signature                                                                                                                                                                                                                                             
----------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [debug_dump_as_scene](#i_debug_dump_as_scene) ( [String](https://docs.godotengine.org/en/stable/classes/class_string.html) path, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) include_instancer ) const                     
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [debug_get_data_block_count](#i_debug_get_data_block_count) ( ) const                                                                                                                                                                                 
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)  | [debug_get_data_block_info](#i_debug_get_data_block_info) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) block_pos, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) const                  
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)              | [debug_get_draw_flag](#i_debug_get_draw_flag) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) flag_index ) const                                                                                                               
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [debug_get_mesh_block_count](#i_debug_get_mesh_block_count) ( ) const                                                                                                                                                                                 
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)  | [debug_get_mesh_block_info](#i_debug_get_mesh_block_info) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) block_pos, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) const                  
[Array](https://docs.godotengine.org/en/stable/classes/class_array.html)            | [debug_get_octrees_detailed](#i_debug_get_octrees_detailed) ( ) const                                                                                                                                                                                 
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)              | [debug_is_draw_enabled](#i_debug_is_draw_enabled) ( ) const                                                                                                                                                                                           
[Array](https://docs.godotengine.org/en/stable/classes/class_array.html)            | [debug_print_sdf_top_down](#i_debug_print_sdf_top_down) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) center, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) extents )             
[Array](https://docs.godotengine.org/en/stable/classes/class_array.html)            | [debug_raycast_mesh_block](#i_debug_raycast_mesh_block) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) dir ) const               
[void](#)                                                                           | [debug_set_draw_enabled](#i_debug_set_draw_enabled) ( [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled )                                                                                                                
[void](#)                                                                           | [debug_set_draw_flag](#i_debug_set_draw_flag) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) flag_index, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled )                                     
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [get_data_block_region_extent](#i_get_data_block_region_extent) ( ) const                                                                                                                                                                             
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [get_data_block_size](#i_get_data_block_size) ( ) const                                                                                                                                                                                               
[VoxelGenerator](VoxelGenerator.md)                                                 | [get_normalmap_generator_override](#i_get_normalmap_generator_override) ( ) const                                                                                                                                                                     
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [get_normalmap_generator_override_begin_lod_index](#i_get_normalmap_generator_override_begin_lod_index) ( ) const                                                                                                                                     
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [get_process_callback](#i_get_process_callback) ( ) const                                                                                                                                                                                             
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)  | [get_statistics](#i_get_statistics) ( ) const                                                                                                                                                                                                         
[VoxelTool](VoxelTool.md)                                                           | [get_voxel_tool](#i_get_voxel_tool) ( )                                                                                                                                                                                                               
[void](#)                                                                           | [save_modified_blocks](#i_save_modified_blocks) ( )                                                                                                                                                                                                   
[void](#)                                                                           | [set_normalmap_generator_override](#i_set_normalmap_generator_override) ( [VoxelGenerator](VoxelGenerator.md) generator_override )                                                                                                                    
[void](#)                                                                           | [set_normalmap_generator_override_begin_lod_index](#i_set_normalmap_generator_override_begin_lod_index) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod_index )                                                            
[void](#)                                                                           | [set_process_callback](#i_set_process_callback) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mode )                                                                                                                         
[Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)      | [voxel_to_data_block_position](#i_voxel_to_data_block_position) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) voxel_position, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod_index ) const 
[Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)      | [voxel_to_mesh_block_position](#i_voxel_to_mesh_block_position) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) voxel_position, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod_index ) const 
<p></p>

## Enumerations: 

enum **ProcessCallback**: 

- **PROCESS_CALLBACK_IDLE** = **0** --- The node will use `_process` for the part of its logic running on the main thread.
- **PROCESS_CALLBACK_PHYSICS** = **1** --- The node will use `_physics_process` for the part of its logic running on the main thread.
- **PROCESS_CALLBACK_DISABLED** = **2** --- The node will not update. Use with caution!

enum **DebugDrawFlag**: 

- **DEBUG_DRAW_OCTREE_NODES** = **0**
- **DEBUG_DRAW_OCTREE_BOUNDS** = **1**
- **DEBUG_DRAW_MESH_UPDATES** = **2**
- **DEBUG_DRAW_EDIT_BOXES** = **3**
- **DEBUG_DRAW_VOLUME_BOUNDS** = **4**
- **DEBUG_DRAW_EDITED_BLOCKS** = **5**
- **DEBUG_DRAW_FLAGS_COUNT** = **6**


## Property Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_collision_layer"></span> **collision_layer** = 1

Collision layer used by generated colliders. Check Godot documentation for more information.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_collision_lod_count"></span> **collision_lod_count** = 0

How many LOD levels are set to generate colliders, starting from LOD 0.

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_collision_margin"></span> **collision_margin** = 0.04

Collision margin used by generated colliders. Note that it may depend on which physics engine is used under the hood, as some don't use margins.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_collision_mask"></span> **collision_mask** = 1

Collision mask used by generated colliders. Check Godot documentation for more information.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_collision_update_delay"></span> **collision_update_delay** = 0

How long to wait before updating colliders after an edit, in milliseconds. Collider generation is expensive, so the intent is to smooth it out.

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_full_load_mode_enabled"></span> **full_load_mode_enabled** = false

If enabled, data streaming will be turned off, and all voxel data will be loaded from the member stream into memory.

This removes several constraints, such as being able to edit anywhere and allowing distant normalmaps to include edited regions. This comes at the expense of more memory usage. However, only edited regions use memory, so in practice it can be good enough.

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_generate_collisions"></span> **generate_collisions** = true

If enabled, chunked colliders will be generated from meshes.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_lod_count"></span> **lod_count** = 4

How many LOD levels to use. This should be tuned alongside member lod_distance: if you want to see very far, you need more LOD levels. This allows blocks to become larger the further away they are, to keep their numbers to an acceptable amount. In contrast, too few LOD levels means regions far away will have to use too many small blocks, which can affect performance.

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_lod_distance"></span> **lod_distance** = 48.0

How far LOD 0 extends from the viewer. Each parent LOD will extend twice as far as their children LOD levels. When member full_load_mode_enabled is disabled, this also defines how far edits are allowed.

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_lod_fade_duration"></span> **lod_fade_duration** = 0.0

When set greater than 0, enables LOD fading. When mesh blocks get split/merged as level of detail changes, they will fade to make the transition less noticeable (or at least more pleasant). This feature requires to use a specific shader, check the online documentation or examples for more information.

- [Material](https://docs.godotengine.org/en/stable/classes/class_material.html)<span id="i_material"></span> **material**

Material used for the surface of the volume. The main usage of this node is with smooth voxels, which means if you want more than one "material" on the ground, you need to use splatmapping techniques with a shader. In addition, many features require shaders to work properly. Check the online documentation or examples for more information.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_mesh_block_size"></span> **mesh_block_size** = 16

Size of meshes used for chunks of this volume, in voxels. Can only be set to either 16 or 32. Using 32 is expected to increase rendering performance, and slightly increase the cost of edits.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_normalmap_begin_lod_index"></span> **normalmap_begin_lod_index** = 2

From which LOD index normalmaps will be generated. There won't be normalmaps below this index.

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_normalmap_enabled"></span> **normalmap_enabled** = false

Enables generation of distant normalmaps. This is a feature used with smooth terrain only (SDF). It is an expensive feature but allows to bring a lot more detail to distant ground.

This feature requires to use a specific shader, check the online documentation or examples for more information.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_normalmap_max_deviation_degrees"></span> **normalmap_max_deviation_degrees** = 60


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_normalmap_octahedral_encoding_enabled"></span> **normalmap_octahedral_encoding_enabled** = false

Enables octahedral compression of normalmaps, which reduces memory usage caused by distant normalmaps by about 33%, with some impact on visual quality. Your shader may be modified accordingly to decode them.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_normalmap_tile_resolution_max"></span> **normalmap_tile_resolution_max** = 8

Maximum resolution of tiles in distant normalmaps.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_normalmap_tile_resolution_min"></span> **normalmap_tile_resolution_min** = 4

Minimum resolution of tiles in distant normalmaps.

This is the resolution at which normalmaps will begin with, at the LOD level defined in member normalmap_begin_lod_index. Resolutions will double at each LOD level, until they reach member normalmap_tile_resolution_max.

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_normalmap_use_gpu"></span> **normalmap_use_gpu** = false


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_run_stream_in_editor"></span> **run_stream_in_editor** = true

Sets wether the member generator and the member stream will run in the editor. This setting may turn on automatically if either contain a script, as multithreading can clash with script reloading in unexpected ways.

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_threaded_update_enabled"></span> **threaded_update_enabled** = false

When enabled, this node will run a large part of its update cycle in a separate thread. Otherwise, it will run on the main thread.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_view_distance"></span> **view_distance** = 512

Maximum distance where terrain will load around viewers. If your terrain size is finite (like for a planet) and you want to keep it in view, you may want to set this value to a very large number. This is mainly useful for infinite terrains.

- [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html)<span id="i_voxel_bounds"></span> **voxel_bounds** = AABB(-5.36871e+08, -5.36871e+08, -5.36871e+08, 1.07374e+09, 1.07374e+09, 1.07374e+09)

Bounds within which volume data can exist (loaded or not), in voxels. By default, it is pseudo-infinite. If you make a planet, island or some sort of arena, you may want to choose a finite size.

Note, because this volume uses chunks with LOD, these bounds will snap to the closest chunk boundary.

## Method Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_debug_dump_as_scene"></span> **debug_dump_as_scene**( [String](https://docs.godotengine.org/en/stable/classes/class_string.html) path, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) include_instancer ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_debug_get_data_block_count"></span> **debug_get_data_block_count**( ) 


- [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)<span id="i_debug_get_data_block_info"></span> **debug_get_data_block_info**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) block_pos, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) 


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_debug_get_draw_flag"></span> **debug_get_draw_flag**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) flag_index ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_debug_get_mesh_block_count"></span> **debug_get_mesh_block_count**( ) 


- [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)<span id="i_debug_get_mesh_block_info"></span> **debug_get_mesh_block_info**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) block_pos, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) 


- [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_debug_get_octrees_detailed"></span> **debug_get_octrees_detailed**( ) 


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_debug_is_draw_enabled"></span> **debug_is_draw_enabled**( ) 


- [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_debug_print_sdf_top_down"></span> **debug_print_sdf_top_down**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) center, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) extents ) 


- [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_debug_raycast_mesh_block"></span> **debug_raycast_mesh_block**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) dir ) 


- [void](#)<span id="i_debug_set_draw_enabled"></span> **debug_set_draw_enabled**( [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled ) 


- [void](#)<span id="i_debug_set_draw_flag"></span> **debug_set_draw_flag**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) flag_index, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_data_block_region_extent"></span> **get_data_block_region_extent**( ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_data_block_size"></span> **get_data_block_size**( ) 

Gets the size of one cunic data block in voxels.

- [VoxelGenerator](VoxelGenerator.md)<span id="i_get_normalmap_generator_override"></span> **get_normalmap_generator_override**( ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_normalmap_generator_override_begin_lod_index"></span> **get_normalmap_generator_override_begin_lod_index**( ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_process_callback"></span> **get_process_callback**( ) 

Gets which callback is used to run the main thread update of this node.

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

Gets an instance of [VoxelTool](VoxelTool.md) bound to this volume. Allows to query and edit voxels.

- [void](#)<span id="i_save_modified_blocks"></span> **save_modified_blocks**( ) 

Requests saving of all modified voxels. Saving is asynchronous and will complete some time in the future. If the game quits, the engine will ensure saving tasks get completed before the application shuts down.

There is currently no reliable way to tell if saving has completed.

Note that blocks getting unloaded as the viewer moves around can also trigger saving tasks, independently from this function.

- [void](#)<span id="i_set_normalmap_generator_override"></span> **set_normalmap_generator_override**( [VoxelGenerator](VoxelGenerator.md) generator_override ) 


- [void](#)<span id="i_set_normalmap_generator_override_begin_lod_index"></span> **set_normalmap_generator_override_begin_lod_index**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod_index ) 


- [void](#)<span id="i_set_process_callback"></span> **set_process_callback**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mode ) 

Sets which process callback is used to run the main thread update of this node. By default, it uses `_process`.

- [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)<span id="i_voxel_to_data_block_position"></span> **voxel_to_data_block_position**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) voxel_position, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod_index ) 

Converts a voxel position into a data block position for a specific LOD index.

- [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)<span id="i_voxel_to_mesh_block_position"></span> **voxel_to_mesh_block_position**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) voxel_position, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod_index ) 

Converts a voxel position into a mesh block position for a specific LOD index.

_Generated on Mar 26, 2023_
