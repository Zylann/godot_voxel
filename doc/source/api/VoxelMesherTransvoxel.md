# VoxelMesherTransvoxel

Inherits: [VoxelMesher](VoxelMesher.md)


Implements isosurface generation (smooth voxels) using the [Transvoxel](https://transvoxel.org/) algorithm.

## Properties: 


Type     | Name                                                                       | Default 
-------- | -------------------------------------------------------------------------- | --------
`bool`   | [mesh_optimization_enabled](#i_mesh_optimization_enabled)                  | false   
`float`  | [mesh_optimization_error_threshold](#i_mesh_optimization_error_threshold)  | 0.005   
`float`  | [mesh_optimization_target_ratio](#i_mesh_optimization_target_ratio)        | 0.0     
`int`    | [texturing_mode](#i_texturing_mode)                                        | 0       
<p></p>

## Methods: 


Return                                                                            | Signature                                                                                                                                                                         
--------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[ArrayMesh](https://docs.godotengine.org/en/stable/classes/class_arraymesh.html)  | [build_transition_mesh](#i_build_transition_mesh) ( [VoxelBuffer](VoxelBuffer.md) voxel_buffer, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) direction )  
<p></p>

## Enumerations: 

enum **TexturingMode**: 

- **TEXTURES_NONE** = **0**
- **TEXTURES_BLEND_4_OVER_16** = **1**


## Property Descriptions

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_mesh_optimization_enabled"></span> **mesh_optimization_enabled** = false


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_mesh_optimization_error_threshold"></span> **mesh_optimization_error_threshold** = 0.005


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_mesh_optimization_target_ratio"></span> **mesh_optimization_target_ratio** = 0.0


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_texturing_mode"></span> **texturing_mode** = 0


## Method Descriptions

- [ArrayMesh](https://docs.godotengine.org/en/stable/classes/class_arraymesh.html)<span id="i_build_transition_mesh"></span> **build_transition_mesh**( [VoxelBuffer](VoxelBuffer.md) voxel_buffer, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) direction ) 


_Generated on Nov 06, 2021_
