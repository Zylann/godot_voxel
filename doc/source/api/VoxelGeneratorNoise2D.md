# VoxelGeneratorNoise2D

Inherits: [VoxelGeneratorHeightmap](VoxelGeneratorHeightmap.md)

Voxel generator producing noise-based heightmap terrain.

## Properties: 


Type                                                                      | Name                             | Default 
------------------------------------------------------------------------- | -------------------------------- | --------
[Curve](https://docs.godotengine.org/en/stable/classes/class_curve.html)  | [curve](#i_curve)                |         
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [height_range](#i_height_range)  | 200.0   
[Noise](https://docs.godotengine.org/en/stable/classes/class_noise.html)  | [noise](#i_noise)                |         
<p></p>

## Property Descriptions

### [Curve](https://docs.godotengine.org/en/stable/classes/class_curve.html)<span id="i_curve"></span> **curve**

When assigned, this curve will alter the distribution of height variations, allowing to give some kind of "profile" to the generated shapes.

By default, a linear curve from 0 to 1 is used.

It is assumed that the curve's domain goes from 0 to 1.

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_height_range"></span> **height_range** = 200.0


### [Noise](https://docs.godotengine.org/en/stable/classes/class_noise.html)<span id="i_noise"></span> **noise**

Noise used to produce the heightmap. It is required for the generator to work.

_Generated on May 15, 2025_
