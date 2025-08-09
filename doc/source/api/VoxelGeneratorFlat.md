# VoxelGeneratorFlat

Inherits: [VoxelGenerator](VoxelGenerator.md)

Voxel generator producing an infinite flat ground.

## Properties: 


Type                                                                      | Name                         | Default         
------------------------------------------------------------------------- | ---------------------------- | ----------------
[ChannelId](VoxelBuffer.md#enumerations)                                  | [channel](#i_channel)        | CHANNEL_SDF (1) 
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [height](#i_height)          | 0.0             
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)      | [voxel_type](#i_voxel_type)  | 1               
<p></p>

## Property Descriptions

### [ChannelId](VoxelBuffer.md#enumerations)<span id="i_channel"></span> **channel** = CHANNEL_SDF (1)

Channel that will be used to generate the ground. Use [VoxelBuffer.CHANNEL_SDF](VoxelBuffer.md#i_CHANNEL_SDF) for smooth terrain, other channels for blocky.

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_height"></span> **height** = 0.0

Altitude of the ground.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_voxel_type"></span> **voxel_type** = 1

If [channel](VoxelGeneratorFlat.md#i_channel) is set to any channel other than [VoxelBuffer.CHANNEL_SDF](VoxelBuffer.md#i_CHANNEL_SDF), this value will be used to fill ground voxels, while air voxels will be set to 0.

_Generated on Aug 09, 2025_
