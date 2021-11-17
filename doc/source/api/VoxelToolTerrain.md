# VoxelToolTerrain

Inherits: [VoxelTool](VoxelTool.md)


Implementation of [VoxelTool](VoxelTool.md) specialized for uses on [VoxelTerrain](VoxelTerrain.md).

## Description: 

It's not a class to instantiate alone, you may get it from [VoxelTerrain](VoxelTerrain.md) using the `get_voxel_tool()` method.

## Methods: 


Return     | Signature                                                                                                                                                                                                                                                                                                                                                                                               
---------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)  | [for_each_voxel_metadata_in_area](#i_for_each_voxel_metadata_in_area) ( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) voxel_area, [FuncRef](https://docs.godotengine.org/en/stable/classes/class_funcref.html) callback )                                                                                                                                                      
[void](#)  | [run_blocky_random_tick](#i_run_blocky_random_tick) ( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) area, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) voxel_count, [FuncRef](https://docs.godotengine.org/en/stable/classes/class_funcref.html) callback, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) batch_count=16 ) const 
<p></p>

## Method Descriptions

- [void](#)<span id="i_for_each_voxel_metadata_in_area"></span> **for_each_voxel_metadata_in_area**( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) voxel_area, [FuncRef](https://docs.godotengine.org/en/stable/classes/class_funcref.html) callback ) 


- [void](#)<span id="i_run_blocky_random_tick"></span> **run_blocky_random_tick**( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) area, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) voxel_count, [FuncRef](https://docs.godotengine.org/en/stable/classes/class_funcref.html) callback, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) batch_count=16 ) 

Picks random voxels within the specified area and executes a function on them. This only works for terrains using [VoxelMesherBlocky](VoxelMesherBlocky.md). Only voxels where member Voxel.random_tickable is `true` will be picked.

_Generated on Nov 06, 2021_
