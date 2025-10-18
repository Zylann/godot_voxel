# VoxelGeneratorWaves

Inherits: [VoxelGeneratorHeightmap](VoxelGeneratorHeightmap.md)

Voxel generator producing a wavy terrain pattern.

## Properties: 


Type                                                                          | Name                                 | Default         
----------------------------------------------------------------------------- | ------------------------------------ | ----------------
[Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html)  | [pattern_offset](#i_pattern_offset)  | Vector2(0, 0)   
[Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html)  | [pattern_size](#i_pattern_size)      | Vector2(30, 30) 
<p></p>

## Property Descriptions

### [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html)<span id="i_pattern_offset"></span> **pattern_offset** = Vector2(0, 0)

Offset (or phase) of the waves.

### [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html)<span id="i_pattern_size"></span> **pattern_size** = Vector2(30, 30)

Length of the waves. Note that this only controls length across the X and Z axes. Height is controlled by [VoxelGeneratorHeightmap.height_start](VoxelGeneratorHeightmap.md#i_height_start) and [VoxelGeneratorHeightmap.height_range](VoxelGeneratorHeightmap.md#i_height_range).

_Generated on Aug 09, 2025_
