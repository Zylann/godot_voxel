# VoxelMesherTransvoxel

Inherits: [VoxelMesher](VoxelMesher.md)


Implements isosurface generation (smooth voxels) using the [Transvoxel](https://transvoxel.org/) algorithm.

## Properties: 


Type   | Name                                 | Default 
------ | ------------------------------------ | --------
`int`  | [texturing_mode](#i_texturing_mode)  | 0       
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

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_texturing_mode"></span> **texturing_mode** = 0


## Method Descriptions

- [ArrayMesh](https://docs.godotengine.org/en/stable/classes/class_arraymesh.html)<span id="i_build_transition_mesh"></span> **build_transition_mesh**( [VoxelBuffer](VoxelBuffer.md) voxel_buffer, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) direction ) 


_Generated on May 31, 2021_
