# VoxelInstancer

Inherits: [Spatial](https://docs.godotengine.org/en/stable/classes/class_spatial.html)


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
----------------------------------------------------------------------------------- | --------------------------------------------------------------------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [debug_get_block_count](#i_debug_get_block_count) ( ) const         
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)  | [debug_get_instance_counts](#i_debug_get_instance_counts) ( ) const 
<p></p>

## Enumerations: 

enum **UpMode**: 

- **UP_MODE_POSITIVE_Y** = **0**
- **UP_MODE_SPHERE** = **1**


## Constants: 

- **MAX_LOD** = **8**

## Property Descriptions

- [VoxelInstanceLibrary](VoxelInstanceLibrary.md)<span id="i_library"></span> **library**


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_up_mode"></span> **up_mode** = 0


## Method Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_debug_get_block_count"></span> **debug_get_block_count**( ) 


- [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)<span id="i_debug_get_instance_counts"></span> **debug_get_instance_counts**( ) 


_Generated on Nov 06, 2021_
