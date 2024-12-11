# VoxelMesherTransvoxel

Inherits: [VoxelMesher](VoxelMesher.md)

Implements isosurface generation (smooth voxels) using the [Transvoxel](https://transvoxel.org/) algorithm.

## Properties: 


Type                                                                      | Name                                                                       | Default 
------------------------------------------------------------------------- | -------------------------------------------------------------------------- | --------
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)    | [deep_sampling_enabled](#i_deep_sampling_enabled)                          | false   
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [edge_clamp_margin](#i_edge_clamp_margin)                                  | 0.02    
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)    | [mesh_optimization_enabled](#i_mesh_optimization_enabled)                  | false   
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [mesh_optimization_error_threshold](#i_mesh_optimization_error_threshold)  | 0.005   
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [mesh_optimization_target_ratio](#i_mesh_optimization_target_ratio)        | 0.0     
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)      | [texturing_mode](#i_texturing_mode)                                        | 0       
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)    | [transitions_enabled](#i_transitions_enabled)                              | true    
<p></p>

## Methods: 


Return                                                                            | Signature                                                                                                                                                                         
--------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[ArrayMesh](https://docs.godotengine.org/en/stable/classes/class_arraymesh.html)  | [build_transition_mesh](#i_build_transition_mesh) ( [VoxelBuffer](VoxelBuffer.md) voxel_buffer, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) direction )  
<p></p>

## Enumerations: 

enum **TexturingMode**: 

- <span id="i_TEXTURES_NONE"></span>**TEXTURES_NONE** = **0**
- <span id="i_TEXTURES_BLEND_4_OVER_16"></span>**TEXTURES_BLEND_4_OVER_16** = **1**


## Property Descriptions

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_deep_sampling_enabled"></span> **deep_sampling_enabled** = false

*(This property has no documentation)*

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_edge_clamp_margin"></span> **edge_clamp_margin** = 0.02

When a marching cube cell is computed, vertices may be placed anywhere on edges of the cell, including very close to corners. This can lead to very thin or small triangles, which can be a problem notably for some physics engines. this margin is the minimum distance from corners, below which vertices will be clamped to it. Increasing this value might reduce quality of the mesh introducing small ridges. This property cannot be lower than 0 (in which case no clamping occurs), and cannot be higher than 0.5 (in which case no interpolation occurs as vertices always get placed in the middle of edges).

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_mesh_optimization_enabled"></span> **mesh_optimization_enabled** = false

*(This property has no documentation)*

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_mesh_optimization_error_threshold"></span> **mesh_optimization_error_threshold** = 0.005

*(This property has no documentation)*

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_mesh_optimization_target_ratio"></span> **mesh_optimization_target_ratio** = 0.0

*(This property has no documentation)*

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_texturing_mode"></span> **texturing_mode** = 0

*(This property has no documentation)*

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_transitions_enabled"></span> **transitions_enabled** = true

*(This property has no documentation)*

## Method Descriptions

### [ArrayMesh](https://docs.godotengine.org/en/stable/classes/class_arraymesh.html)<span id="i_build_transition_mesh"></span> **build_transition_mesh**( [VoxelBuffer](VoxelBuffer.md) voxel_buffer, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) direction ) 

*(This method has no documentation)*

_Generated on Aug 27, 2024_
