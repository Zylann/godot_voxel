# VoxelGenerator

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)


Base class to all voxel procedural generators. If you want to define a custom one with a script, this is the class you should extend from. All implementations must be thread safe.

## Methods: 


Return     | Signature                                                                                                                                                                                                                                                  
---------- | -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)  | [generate_block](#i_generate_block) ( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod )  
<p></p>

## Method Descriptions

- [void](#)<span id="i_generate_block"></span> **generate_block**( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) 

Generates a block of voxels within the specified world area.

`out_buffer`: Buffer in which voxel data will be generated. It should not be `null` and should be given the requested size. Do not keep a reference on it after the call.

`origin_in_voxels`: Coordinates of the lower corner of the box to generate, relative to LOD0.

`lod`: Level of detail index to use for this block. Some generators might not support LOD, in which case it can be left 0.

_Generated on Nov 06, 2021_
