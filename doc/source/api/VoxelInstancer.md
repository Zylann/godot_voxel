# VoxelInstancer

Inherits: [Node3D](https://docs.godotengine.org/en/stable/classes/class_node3d.html)

Spawns items on top of voxel surfaces.

## Description: 

Add-on to voxel nodes, allowing to spawn static elements on the surface. These elements are rendered with hardware instancing, can have collisions, and also be persistent. It must be child of a voxel node.

## Properties: 


Type                                                                      | Name                                                                             | Default                
------------------------------------------------------------------------- | -------------------------------------------------------------------------------- | -----------------------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)      | [collision_update_budget_microseconds](#i_collision_update_budget_microseconds)  | 500                    
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [fading_duration](#i_fading_duration)                                            | 0.3                    
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)    | [fading_enabled](#i_fading_enabled)                                              | false                  
[VoxelInstanceLibrary](VoxelInstanceLibrary.md)                           | [library](#i_library)                                                            |                        
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)      | [mesh_lod_update_budget_microseconds](#i_mesh_lod_update_budget_microseconds)    | 500                    
[UpMode](VoxelInstancer.md#enumerations)                                  | [up_mode](#i_up_mode)                                                            | UP_MODE_POSITIVE_Y (0) 
<p></p>

## Methods: 


Return                                                                              | Signature                                                                                                                                                                                                                             
----------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                                           | [debug_dump_as_scene](#i_debug_dump_as_scene) ( [String](https://docs.godotengine.org/en/stable/classes/class_string.html) fpath ) const                                                                                              
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [debug_get_block_count](#i_debug_get_block_count) ( ) const                                                                                                                                                                           
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)  | [debug_get_block_infos](#i_debug_get_block_infos) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) world_position, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) item_id )       
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)              | [debug_get_draw_flag](#i_debug_get_draw_flag) ( [DebugDrawFlag](VoxelInstancer.md#enumerations) flag ) const                                                                                                                          
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)  | [debug_get_instance_counts](#i_debug_get_instance_counts) ( ) const                                                                                                                                                                   
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)              | [debug_is_draw_enabled](#i_debug_is_draw_enabled) ( ) const                                                                                                                                                                           
[void](#)                                                                           | [debug_set_draw_enabled](#i_debug_set_draw_enabled) ( [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled )                                                                                                
[void](#)                                                                           | [debug_set_draw_flag](#i_debug_set_draw_flag) ( [DebugDrawFlag](VoxelInstancer.md#enumerations) flag, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled )                                                
[void](#)                                                                           | [remove_instances_in_sphere](#i_remove_instances_in_sphere) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) center, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) radius )  
<p></p>

## Enumerations: 

enum **UpMode**: 

- <span id="i_UP_MODE_POSITIVE_Y"></span>**UP_MODE_POSITIVE_Y** = **0** --- Up is towards the positive Y axis. This is the default assumption in Godot.
- <span id="i_UP_MODE_SPHERE"></span>**UP_MODE_SPHERE** = **1** --- Up is opposite from the direction where the terrain's origin is. May be used if your terrain is a planet for example.

enum **DebugDrawFlag**: 

- <span id="i_DEBUG_DRAW_ALL_BLOCKS"></span>**DEBUG_DRAW_ALL_BLOCKS** = **0**
- <span id="i_DEBUG_DRAW_EDITED_BLOCKS"></span>**DEBUG_DRAW_EDITED_BLOCKS** = **1**
- <span id="i_DEBUG_DRAW_FLAGS_COUNT"></span>**DEBUG_DRAW_FLAGS_COUNT** = **2**


## Constants: 

- <span id="i_MAX_LOD"></span>**MAX_LOD** = **8**

## Property Descriptions

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_collision_update_budget_microseconds"></span> **collision_update_budget_microseconds** = 500

Hints the maximum time that should be spent updating collisions that are based on distance (see [VoxelInstanceLibraryMultiMeshItem.collision_distance](VoxelInstanceLibraryMultiMeshItem.md#i_collision_distance)). This time can still be exceeded if an operation takes longer, but then no further processing will occur until next frame.

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_fading_duration"></span> **fading_duration** = 0.3

How long it takes for fading to complete, in seconds (same for fading in and out). Used when [fading_enabled](VoxelInstancer.md#i_fading_enabled) is active.

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_fading_enabled"></span> **fading_enabled** = false

When enabled, a per-instance shader parameter will be updated when instances load in and out, so it can be used to apply a fading effect.

The material used by instances must have a shader that has a `instance uniform vec2 u_lod_fade;` property. X will animate from 0 to 1 during fade in and fade out. Y will be 1 when fading in, and 0 when fading out.

### [VoxelInstanceLibrary](VoxelInstanceLibrary.md)<span id="i_library"></span> **library**

Library from which instances to spawn will be taken from.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_mesh_lod_update_budget_microseconds"></span> **mesh_lod_update_budget_microseconds** = 500

Hints the maximum time that should be spent updating mesh LODs (see "mesh LOD" properties on [VoxelInstanceLibraryMultiMeshItem](VoxelInstanceLibraryMultiMeshItem.md)). This time can still be exceeded if an operation takes longer, but then no further processing will occur until next frame.

### [UpMode](VoxelInstancer.md#enumerations)<span id="i_up_mode"></span> **up_mode** = UP_MODE_POSITIVE_Y (0)

Where to consider the "up" direction is on the terrain when generating instances. See also [VoxelInstanceGenerator](VoxelInstanceGenerator.md).

## Method Descriptions

### [void](#)<span id="i_debug_dump_as_scene"></span> **debug_dump_as_scene**( [String](https://docs.godotengine.org/en/stable/classes/class_string.html) fpath ) 

*(This method has no documentation)*

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_debug_get_block_count"></span> **debug_get_block_count**( ) 

*(This method has no documentation)*

### [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)<span id="i_debug_get_block_infos"></span> **debug_get_block_infos**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) world_position, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) item_id ) 

*(This method has no documentation)*

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_debug_get_draw_flag"></span> **debug_get_draw_flag**( [DebugDrawFlag](VoxelInstancer.md#enumerations) flag ) 

*(This method has no documentation)*

### [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)<span id="i_debug_get_instance_counts"></span> **debug_get_instance_counts**( ) 

*(This method has no documentation)*

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_debug_is_draw_enabled"></span> **debug_is_draw_enabled**( ) 

*(This method has no documentation)*

### [void](#)<span id="i_debug_set_draw_enabled"></span> **debug_set_draw_enabled**( [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled ) 

*(This method has no documentation)*

### [void](#)<span id="i_debug_set_draw_flag"></span> **debug_set_draw_flag**( [DebugDrawFlag](VoxelInstancer.md#enumerations) flag, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled ) 

*(This method has no documentation)*

### [void](#)<span id="i_remove_instances_in_sphere"></span> **remove_instances_in_sphere**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) center, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) radius ) 

Removes all instances having their origin inside the given sphere. Coordinates are local to the instancer.

_Generated on Aug 09, 2025_
