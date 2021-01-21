# VoxelGenerator

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)


Base class to all voxel procedural generators. If you want to define a custom one with a script, this is the class you should extend from.

Important: this engine makes heavy use of threads. Generators will run in one of them, so make sure you don't access the scene tree or other unsafe APIs from within a generator.

## Methods: 


Return     | Signature                                                                                                                                                                                                                                                  
---------- | -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)  | [generate_block](#i_generate_block) ( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod )  
<p></p>

## Method Descriptions

- [void](#)<span id="i_generate_block"></span> **generate_block**( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) 

Generates a block of voxels within the specified world area.

_Generated on Jan 21, 2021_
