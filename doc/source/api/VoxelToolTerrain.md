# VoxelToolTerrain

Inherits: [VoxelTool](VoxelTool.md)

Implementation of [VoxelTool](VoxelTool.md) specialized for uses on [VoxelTerrain](VoxelTerrain.md).

## Description: 

Functions in this class are specific to [VoxelTerrain](VoxelTerrain.md). For generic functions, you may also check [VoxelTool](VoxelTool.md).

It's not a class to instantiate alone, you may get it from [VoxelTerrain](VoxelTerrain.md) using the `get_voxel_tool()` method.

## Methods: 


Return     | Signature                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       
---------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)  | [do_hemisphere](#i_do_hemisphere) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) center, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) radius, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) flat_direction, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) smoothness=0.0 )                                                                                                
[void](#)  | [for_each_voxel_metadata_in_area](#i_for_each_voxel_metadata_in_area) ( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) voxel_area, [Callable](https://docs.godotengine.org/en/stable/classes/class_callable.html) callback )                                                                                                                                                                                                                                            
[void](#)  | [run_blocky_random_tick](#i_run_blocky_random_tick) ( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) area, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) voxel_count, [Callable](https://docs.godotengine.org/en/stable/classes/class_callable.html) callback, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) batch_count=16, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) tags_mask=4294967295 )  
<p></p>

## Method Descriptions

### [void](#)<span id="i_do_hemisphere"></span> **do_hemisphere**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) center, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) radius, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) flat_direction, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) smoothness=0.0 ) 

Operates on a hemisphere, where `flat_direction` is pointing away from the flat surface (like a normal). `smoothness` determines how the flat part blends with the rounded part, with higher values producing softer more rounded edge.

### [void](#)<span id="i_for_each_voxel_metadata_in_area"></span> **for_each_voxel_metadata_in_area**( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) voxel_area, [Callable](https://docs.godotengine.org/en/stable/classes/class_callable.html) callback ) 

Executes a function for each voxel holding metadata in the given area.

The given callback takes two arguments: voxel position (Vector3i), voxel metadata (Variant).

IMPORTANT: inserting new or removing metadata from inside this function is not allowed.

### [void](#)<span id="i_run_blocky_random_tick"></span> **run_blocky_random_tick**( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) area, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) voxel_count, [Callable](https://docs.godotengine.org/en/stable/classes/class_callable.html) callback, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) batch_count=16, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) tags_mask=4294967295 ) 

Picks random voxels within the specified area. If voxel models have [VoxelBlockyModel.random_tickable](VoxelBlockyModel.md#i_random_tickable) set to `true` and [VoxelBlockyModel.tags_mask](VoxelBlockyModel.md#i_tags_mask) matches any bit in `tags_mask`, executes a function on them. This only works for terrains using [VoxelMesherBlocky](VoxelMesherBlocky.md).

The given callback takes two arguments: voxel position (Vector3i), voxel value (int).


The purpose of `batch_count` is to optimize the picking process through the internal data structure. The algorithm goes as follows: `voxel_count` is divided in batches of length `batch_count`. For each batch, a random block intersecting `area` is chosen, and `batch_count` voxels are picked at random inside of it. If `voxel_count` is not divisible by `batch_count`, an extra block will be picked to do the remainder.

`batch` can bias randomness by concentrating picks in specific blocks, but if this function is used every frame over time, that bias should average out. If you want no bias at all, set `batch_count` to 1.

_Generated on Jan 26, 2026_
