# VoxelGeneratorFlat

Inherits: [VoxelGenerator](VoxelGenerator.md)

Voxel generator producing an infinite flat ground.

## Properties: 


Type                                                                      | Name                         | Default 
------------------------------------------------------------------------- | ---------------------------- | --------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)      | [channel](#i_channel)        | 1       
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [height](#i_height)          | 0.0     
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)      | [voxel_type](#i_voxel_type)  | 1       
<p></p>

## Property Descriptions

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_channel"></span> **channel** = 1

Channel that will be used to generate the ground.

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_height"></span> **height** = 0.0

Altitude of the ground.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_voxel_type"></span> **voxel_type** = 1

If [VoxelGeneratorFlat.channel](VoxelGeneratorFlat.md#i_channel) is set to [VoxelBuffer.CHANNEL_TYPE](VoxelBuffer.md#i_CHANNEL_TYPE), this value will be used to fill ground voxels.

_Generated on Apr 06, 2024_
