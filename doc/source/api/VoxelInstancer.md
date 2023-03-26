# VoxelInstancer

Inherits: [Node3D](https://docs.godotengine.org/en/stable/classes/class_node3d.html)


Spawns items on top of voxel surfaces.

## Description: 

Add-on to voxel nodes, allowing to spawn elements on the surface. These elements are rendered with hardware instancing, can have collisions, and also be persistent. It must be child of a voxel node. At the moment it only supports `VoxelLodTerrain`.

## Properties: 


Type                    | Name                   | Default 
----------------------- | ---------------------- | --------
`VoxelInstanceLibrary`  | [library](#i_library)  |         
`int`                   | [up_mode](#i_up_mode)  | 0       
<p></p>

## Methods: 


Return                                                                              | Signature                                                                                                                                                                                                    
----------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                                           | [debug_dump_as_scene](#i_debug_dump_as_scene) ( [String](https://docs.godotengine.org/en/stable/classes/class_string.html) fpath ) const                                                                     
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [debug_get_block_count](#i_debug_get_block_count) ( ) const                                                                                                                                                  
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)              | [debug_get_draw_flag](#i_debug_get_draw_flag) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) flag ) const                                                                            
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)  | [debug_get_instance_counts](#i_debug_get_instance_counts) ( ) const                                                                                                                                          
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)              | [debug_is_draw_enabled](#i_debug_is_draw_enabled) ( ) const                                                                                                                                                  
[void](#)                                                                           | [debug_set_draw_enabled](#i_debug_set_draw_enabled) ( [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled )                                                                       
[void](#)                                                                           | [debug_set_draw_flag](#i_debug_set_draw_flag) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) flag, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled )  
<p></p>

## Enumerations: 

enum **UpMode**: 

- **UP_MODE_POSITIVE_Y** = **0**
- **UP_MODE_SPHERE** = **1**

enum **DebugDrawFlag**: 

- **DEBUG_DRAW_ALL_BLOCKS** = **0**
- **DEBUG_DRAW_EDITED_BLOCKS** = **1**
- **DEBUG_DRAW_FLAGS_COUNT** = **2**


## Constants: 

- **MAX_LOD** = **8**

## Property Descriptions

- [VoxelInstanceLibrary](VoxelInstanceLibrary.md)<span id="i_library"></span> **library**


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_up_mode"></span> **up_mode** = 0


## Method Descriptions

- [void](#)<span id="i_debug_dump_as_scene"></span> **debug_dump_as_scene**( [String](https://docs.godotengine.org/en/stable/classes/class_string.html) fpath ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_debug_get_block_count"></span> **debug_get_block_count**( ) 


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_debug_get_draw_flag"></span> **debug_get_draw_flag**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) flag ) 


- [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)<span id="i_debug_get_instance_counts"></span> **debug_get_instance_counts**( ) 


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_debug_is_draw_enabled"></span> **debug_is_draw_enabled**( ) 


- [void](#)<span id="i_debug_set_draw_enabled"></span> **debug_set_draw_enabled**( [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled ) 


- [void](#)<span id="i_debug_set_draw_flag"></span> **debug_set_draw_flag**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) flag, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled ) 


_Generated on Mar 26, 2023_
