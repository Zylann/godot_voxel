# VoxelGeneratorNoise2D

Inherits: [VoxelGeneratorHeightmap](VoxelGeneratorHeightmap.md)


Voxel generator producing noise-based heightmap terrain.

## Properties: 


Type     | Name                             | Default 
-------- | -------------------------------- | --------
`Curve`  | [curve](#i_curve)                |         
`float`  | [height_range](#i_height_range)  | 200.0   
`Noise`  | [noise](#i_noise)                |         
<p></p>

## Property Descriptions

- [Curve](https://docs.godotengine.org/en/stable/classes/class_curve.html)<span id="i_curve"></span> **curve**

When assigned, this curve will alter the distribution of height variations, allowing to give some kind of "profile" to the generated shapes.

By default, a linear curve from 0 to 1 is used.

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_height_range"></span> **height_range** = 200.0


- [Noise](https://docs.godotengine.org/en/stable/classes/class_noise.html)<span id="i_noise"></span> **noise**

Noise used to produce the heightmap. It is required for the generator to work.

_Generated on Mar 26, 2023_
