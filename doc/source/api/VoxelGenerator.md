# VoxelGenerator

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)

Inherited by: [VoxelGeneratorFlat](VoxelGeneratorFlat.md), [VoxelGeneratorGraph](VoxelGeneratorGraph.md), [VoxelGeneratorHeightmap](VoxelGeneratorHeightmap.md), [VoxelGeneratorMultipassCB](VoxelGeneratorMultipassCB.md), [VoxelGeneratorNoise](VoxelGeneratorNoise.md), [VoxelGeneratorScript](VoxelGeneratorScript.md)

Base class to all voxel procedural generators.

## Methods: 


Return     | Signature                                                                                                                                                                                                                                                  
---------- | -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)  | [generate_block](#i_generate_block) ( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod )  
<p></p>

## Method Descriptions

### [void](#)<span id="i_generate_block"></span> **generate_block**( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) 

Generates a block of voxels within the specified world area.

`out_buffer`: Buffer in which voxel data will be generated. It must not be `null` and must have a non-empty size.

`origin_in_voxels`: Coordinates of the lower corner of the box to generate, relative to LOD0.

`lod`: Level of detail index to use for this block. Some generators might not support LOD, in which case it can be left 0. At LOD 0, each cell of the passed buffer spans 1 space unit. At LOD 1, 2 units. At LOD 2, 4 units, and so on.

_Generated on Apr 06, 2024_
