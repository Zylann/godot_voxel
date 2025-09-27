# VoxelGeneratorHeightmap

Inherits: [VoxelGenerator](VoxelGenerator.md)

Inherited by: [VoxelGeneratorImage](VoxelGeneratorImage.md), [VoxelGeneratorNoise2D](VoxelGeneratorNoise2D.md), [VoxelGeneratorWaves](VoxelGeneratorWaves.md)

Base class for several basic height-based generators.

## Properties: 


Type                                                                            | Name                             | Default         
------------------------------------------------------------------------------- | -------------------------------- | ----------------
[ChannelId](VoxelBuffer.md#enumerations)                                        | [channel](#i_channel)            | CHANNEL_SDF (1) 
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)        | [height_range](#i_height_range)  | 30.0            
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)        | [height_start](#i_height_start)  | -50.0           
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)        | [iso_scale](#i_iso_scale)        | 1.0             
[Vector2i](https://docs.godotengine.org/en/stable/classes/class_vector2i.html)  | [offset](#i_offset)              | Vector2i(0, 0)  
<p></p>

## Property Descriptions

### [ChannelId](VoxelBuffer.md#enumerations)<span id="i_channel"></span> **channel** = CHANNEL_SDF (1)

Channel where voxels will be generated. If set to [VoxelBuffer.CHANNEL_SDF](VoxelBuffer.md#i_CHANNEL_SDF), voxels will be a signed distance field usable by smooth meshers. Otherwise, the value 1 will be set below ground, and the value 0 will be set above ground (blocky).

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_height_range"></span> **height_range** = 30.0

Maximum distance between the lowest and highest surface points that can generate. 

NOTE: due to a bug in Godot's documentation tool, the default value shown here is not 30.0, but 200.0. This seems to be because one of the subclasses, `VoxelGeneratorWaves`, has a different default value, chosen for better practical results. This property also appears in some subclasses now, despite being defined in the base class.

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_height_start"></span> **height_start** = -50.0

Minimum height where the surface will generate.

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_iso_scale"></span> **iso_scale** = 1.0

Scale applied to the signed distance field when using a smooth terrain configuration.

### [Vector2i](https://docs.godotengine.org/en/stable/classes/class_vector2i.html)<span id="i_offset"></span> **offset** = Vector2i(0, 0)

Offsets height generation along the X and Z axes.

_Generated on Aug 09, 2025_
