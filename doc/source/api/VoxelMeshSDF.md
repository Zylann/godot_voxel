# VoxelMeshSDF

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)




## Properties: 


Type          | Name                                                       | Default 
------------- | ---------------------------------------------------------- | --------
`Dictionary`  | [_data](#i__data)                                          | {}      
`int`         | [bake_mode](#i_bake_mode)                                  | 1       
`bool`        | [boundary_sign_fix_enabled](#i_boundary_sign_fix_enabled)  | true    
`int`         | [cell_count](#i_cell_count)                                | 64      
`float`       | [margin_ratio](#i_margin_ratio)                            | 0.25    
`Mesh`        | [mesh](#i_mesh)                                            |         
`int`         | [partition_subdiv](#i_partition_subdiv)                    | 32      
<p></p>

## Methods: 


Return                                                                    | Signature                                                                                                                    
------------------------------------------------------------------------- | -----------------------------------------------------------------------------------------------------------------------------
[void](#)                                                                 | [bake](#i_bake) ( )                                                                                                          
[void](#)                                                                 | [bake_async](#i_bake_async) ( [SceneTree](https://docs.godotengine.org/en/stable/classes/class_scenetree.html) scene_tree )  
[Array](https://docs.godotengine.org/en/stable/classes/class_array.html)  | [debug_check_sdf](#i_debug_check_sdf) ( [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html) mesh )        
[AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html)    | [get_aabb](#i_get_aabb) ( ) const                                                                                            
[VoxelBuffer](VoxelBuffer.md)                                             | [get_voxel_buffer](#i_get_voxel_buffer) ( ) const                                                                            
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)    | [is_baked](#i_is_baked) ( ) const                                                                                            
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)    | [is_baking](#i_is_baking) ( ) const                                                                                          
<p></p>

## Signals: 

- baked( ) 

## Enumerations: 

enum **BakeMode**: 

- **BAKE_MODE_ACCURATE_NAIVE** = **0**
- **BAKE_MODE_ACCURATE_PARTITIONED** = **1**
- **BAKE_MODE_APPROX_INTERP** = **2**
- **BAKE_MODE_APPROX_FLOODFILL** = **3**
- **BAKE_MODE_COUNT** = **4**


## Property Descriptions

- [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)<span id="i__data"></span> **_data** = {}


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_bake_mode"></span> **bake_mode** = 1


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_boundary_sign_fix_enabled"></span> **boundary_sign_fix_enabled** = true


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_cell_count"></span> **cell_count** = 64


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_margin_ratio"></span> **margin_ratio** = 0.25


- [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_mesh"></span> **mesh**


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_partition_subdiv"></span> **partition_subdiv** = 32


## Method Descriptions

- [void](#)<span id="i_bake"></span> **bake**( ) 


- [void](#)<span id="i_bake_async"></span> **bake_async**( [SceneTree](https://docs.godotengine.org/en/stable/classes/class_scenetree.html) scene_tree ) 


- [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_debug_check_sdf"></span> **debug_check_sdf**( [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html) mesh ) 


- [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html)<span id="i_get_aabb"></span> **get_aabb**( ) 


- [VoxelBuffer](VoxelBuffer.md)<span id="i_get_voxel_buffer"></span> **get_voxel_buffer**( ) 


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_is_baked"></span> **is_baked**( ) 


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_is_baking"></span> **is_baking**( ) 


_Generated on Mar 26, 2023_
