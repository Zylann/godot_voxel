# VoxelMesherTransvoxel

Inherits: [VoxelMesher](VoxelMesher.md)

Implements isosurface generation (smooth voxels) using the [Transvoxel](https://transvoxel.org/) algorithm.

## Properties: 


Type                                                                      | Name                                                                       | Default 
------------------------------------------------------------------------- | -------------------------------------------------------------------------- | --------
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [edge_clamp_margin](#i_edge_clamp_margin)                                  | 0.02    
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)    | [mesh_optimization_enabled](#i_mesh_optimization_enabled)                  | false   
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [mesh_optimization_error_threshold](#i_mesh_optimization_error_threshold)  | 0.005   
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [mesh_optimization_target_ratio](#i_mesh_optimization_target_ratio)        | 0.0     
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)    | [textures_ignore_air_voxels](#i_textures_ignore_air_voxels)                | false   
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

- <span id="i_TEXTURES_NONE"></span>**TEXTURES_NONE** = **0** --- Disables texturing information. This mode is the fastest if you can use a shader to apply textures procedurally.
- <span id="i_TEXTURES_BLEND_4_OVER_16"></span>**TEXTURES_BLEND_4_OVER_16** = **1** --- Adds texturing information as 4 texture indices and 4 weights, encoded in `CUSTOM1.xy` in Godot fragment shaders, where x and y contain 4 packed 8-bit values. Expects voxels to have 4 4-bit indices packed in 16-bit values in [VoxelBuffer.CHANNEL_INDICES](VoxelBuffer.md#i_CHANNEL_INDICES), and 4 4-bit weights in [VoxelBuffer.CHANNEL_WEIGHTS](VoxelBuffer.md#i_CHANNEL_WEIGHTS). In cases where more than 4 textures cross each other in a 2x2x2 voxel area, triangles in that area will only use the 4 indices with the highest weights. A custom shader is required to render this, usually with texture arrays to index textures easily.


## Property Descriptions

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_edge_clamp_margin"></span> **edge_clamp_margin** = 0.02

When a marching cube cell is computed, vertices may be placed anywhere on edges of the cell, including very close to corners. This can lead to very thin or small triangles, which can be a problem notably for some physics engines. this margin is the minimum distance from corners, below which vertices will be clamped to it. Increasing this value might reduce quality of the mesh introducing small ridges. This property cannot be lower than 0 (in which case no clamping occurs), and cannot be higher than 0.5 (in which case no interpolation occurs as vertices always get placed in the middle of edges).

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_mesh_optimization_enabled"></span> **mesh_optimization_enabled** = false

*(This property has no documentation)*

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_mesh_optimization_error_threshold"></span> **mesh_optimization_error_threshold** = 0.005

*(This property has no documentation)*

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_mesh_optimization_target_ratio"></span> **mesh_optimization_target_ratio** = 0.0

*(This property has no documentation)*

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_textures_ignore_air_voxels"></span> **textures_ignore_air_voxels** = false

*(This property has no documentation)*

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_texturing_mode"></span> **texturing_mode** = 0

*(This property has no documentation)*

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_transitions_enabled"></span> **transitions_enabled** = true

*(This property has no documentation)*

## Method Descriptions

### [ArrayMesh](https://docs.godotengine.org/en/stable/classes/class_arraymesh.html)<span id="i_build_transition_mesh"></span> **build_transition_mesh**( [VoxelBuffer](VoxelBuffer.md) voxel_buffer, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) direction ) 

Generates only the part of the mesh that Transvoxel uses to connect surfaces with different level of detail. This method is mainly for testing purposes.

_Generated on Mar 23, 2025_
