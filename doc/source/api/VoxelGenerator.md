# VoxelGenerator

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)


Base class to all voxel procedural generators.

## Methods: 


Return     | Signature                                                                                                                                                                                                                                                  
---------- | -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)  | [generate_block](#i_generate_block) ( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod )  
<p></p>

## Method Descriptions

- [void](#)<span id="i_generate_block"></span> **generate_block**( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) 

Generates a block of voxels within the specified world area.

`out_buffer`: Buffer in which voxel data will be generated. It should not be `null` and should have a non-empty size. Do not keep a reference on it after the call. Note: this buffer can have any non-empty size, but some assumptions can be made depending on which terrain node you're using. [VoxelTerrain](VoxelTerrain.md) will always request blocks of size 16x16x16, but [VoxelLodTerrain](VoxelLodTerrain.md) can request blocks of different sizes.

`origin_in_voxels`: Coordinates of the lower corner of the box to generate, relative to LOD0.

`lod`: Level of detail index to use for this block. Some generators might not support LOD, in which case it can be left 0. At LOD 0, each cell of the passed buffer spans 1 space unit. At LOD 1, 2 units. At LOD 2, 4 units, and so on.

_Generated on Mar 26, 2023_
