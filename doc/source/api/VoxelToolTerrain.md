# VoxelToolTerrain

Inherits: [VoxelTool](VoxelTool.md)


Implementation of [VoxelTool](VoxelTool.md) optimized for uses on [VoxelTerrain](VoxelTerrain.md).

## Methods: 


Return     | Signature                                                                                                                                                                                                                                                                                                                                                                                               
---------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)  | [run_blocky_random_tick](#i_run_blocky_random_tick) ( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) area, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) voxel_count, [FuncRef](https://docs.godotengine.org/en/stable/classes/class_funcref.html) callback, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) batch_count=16 ) const 
<p></p>

## Method Descriptions

- [void](#)<span id="i_run_blocky_random_tick"></span> **run_blocky_random_tick**( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) area, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) voxel_count, [FuncRef](https://docs.godotengine.org/en/stable/classes/class_funcref.html) callback, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) batch_count=16 ) 

Picks random voxels within the specified area and executes a function on them. This only works for terrains using [VoxelMesherBlocky](VoxelMesherBlocky.md). Only voxels where member Voxel.random_tickable is `true` will be picked.

_Generated on May 31, 2021_
