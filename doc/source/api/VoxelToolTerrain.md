# VoxelToolTerrain

Inherits: [VoxelTool](VoxelTool.md)


Implementation of [VoxelTool](VoxelTool.md) specialized for uses on [VoxelTerrain](VoxelTerrain.md).

## Description: 

It's not a class to instantiate alone, you may get it from [VoxelTerrain](VoxelTerrain.md) using the `get_voxel_tool()` method.

## Methods: 


Return     | Signature                                                                                                                                                                                                                                                                                                                                                                                            
---------- | -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)  | [do_hemisphere](#i_do_hemisphere) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) center, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) radius, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) flat_direction, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) smoothness=0.0 )     
[void](#)  | [for_each_voxel_metadata_in_area](#i_for_each_voxel_metadata_in_area) ( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) voxel_area, [Callable](https://docs.godotengine.org/en/stable/classes/class_callable.html) callback )                                                                                                                                                 
[void](#)  | [run_blocky_random_tick](#i_run_blocky_random_tick) ( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) area, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) voxel_count, [Callable](https://docs.godotengine.org/en/stable/classes/class_callable.html) callback, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) batch_count=16 )  
<p></p>

## Method Descriptions

- [void](#)<span id="i_do_hemisphere"></span> **do_hemisphere**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) center, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) radius, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) flat_direction, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) smoothness=0.0 ) 


- [void](#)<span id="i_for_each_voxel_metadata_in_area"></span> **for_each_voxel_metadata_in_area**( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) voxel_area, [Callable](https://docs.godotengine.org/en/stable/classes/class_callable.html) callback ) 

Executes a function for each voxel holding metadata in the given area.

The given callback takes two arguments: voxel position (Vector3i), voxel metadata (Variant).

IMPORTANT: inserting new or removing metadata from inside this function is not allowed.

- [void](#)<span id="i_run_blocky_random_tick"></span> **run_blocky_random_tick**( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) area, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) voxel_count, [Callable](https://docs.godotengine.org/en/stable/classes/class_callable.html) callback, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) batch_count=16 ) 

Picks random voxels within the specified area and executes a function on them. This only works for terrains using [VoxelMesherBlocky](VoxelMesherBlocky.md). Only voxels where member Voxel.random_tickable is `true` will be picked.

The given callback takes two arguments: voxel position (Vector3i), voxel value (int).

_Generated on Mar 26, 2023_
