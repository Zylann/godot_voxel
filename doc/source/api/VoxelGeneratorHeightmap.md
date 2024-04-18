# VoxelGeneratorHeightmap

Inherits: [VoxelGenerator](VoxelGenerator.md)

Inherited by: [VoxelGeneratorImage](VoxelGeneratorImage.md), [VoxelGeneratorNoise2D](VoxelGeneratorNoise2D.md), [VoxelGeneratorWaves](VoxelGeneratorWaves.md)

## Properties: 


Type                                                                      | Name                             | Default 
------------------------------------------------------------------------- | -------------------------------- | --------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)      | [channel](#i_channel)            | 1       
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [height_range](#i_height_range)  | 30.0    
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [height_start](#i_height_start)  | -50.0   
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [iso_scale](#i_iso_scale)        | 1.0     
<p></p>

## Property Descriptions

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_channel"></span> **channel** = 1

*(This property has no documentation)*

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_height_range"></span> **height_range** = 30.0

Maximum distance between the lowest and highest surface points that can generate. 

NOTE: due to a bug in Godot's documentation tool, the default value shown here is not 30.0, but 200.0. This seems to be because one of the subclasses, `VoxelGeneratorWaves`, has a different default value, chosen for better practical results. This property also appears in some subclasses now, despite being defined in the base class.

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_height_start"></span> **height_start** = -50.0

Minimum height where the surface will generate.

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_iso_scale"></span> **iso_scale** = 1.0

Scale applied to the signed distance field. This is useful when smooth voxels are used, to reduce blockyness over large distances.

_Generated on Apr 06, 2024_
