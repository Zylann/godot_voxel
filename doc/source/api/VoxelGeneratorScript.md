# VoxelGeneratorScript

Inherits: [VoxelGenerator](VoxelGenerator.md)


Base class for custom generators defined with a script.

Important: this engine makes heavy use of threads. Generators will run in one of them, so make sure you don't access the scene tree or other unsafe APIs from within a generator.

## Methods: 


Return                                                                | Signature                                                                                                                                                                                                                                                           
--------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                             | [_generate_block](#i__generate_block) ( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) virtual 
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [_get_used_channels_mask](#i__get_used_channels_mask) ( ) virtual                                                                                                                                                                                                   
<p></p>

## Method Descriptions

- [void](#)<span id="i__generate_block"></span> **_generate_block**( [VoxelBuffer](VoxelBuffer.md) out_buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin_in_voxels, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) lod ) 

`out_buffer`: Buffer in which to populate voxel data. It will never be `null` and will have the requested size. It is only valid for this function, do not store it anywhere after the end.

`origin_in_voxels`: Coordinates of the lower corner of the box to generate, relative to LOD0. The size of the box is known from `out_buffer`.

`lod`: Level of detail index to use for this block. It can be ignored if you don't use LOD. This may be used as a power of two, telling how big is one voxel. For example, if you use a loop to fill the buffer using noise, you should sample that noise at steps of 2^lod, starting from `origin_in_voxels` (in code you can use `1 << lod` for fast computation, instead of `pow(2, lod)`). You may want to separate variables that iterate the coordinates in `out_buffer` and variables used to generate voxel values in space.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i__get_used_channels_mask"></span> **_get_used_channels_mask**( ) 

Use this to indicate which channels your generator will use. It returns a bitmask, so for example you may provide information like this: `(1 << channel1) | (1 << channel2)`

_Generated on Nov 06, 2021_
