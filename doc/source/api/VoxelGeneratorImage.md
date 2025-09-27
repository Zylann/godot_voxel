# VoxelGeneratorImage

Inherits: [VoxelGeneratorHeightmap](VoxelGeneratorHeightmap.md)

Voxel generator producing a heightmap-based shape using an image.

## Description: 

Uses the red channel of an image to generate a heightmap, such that the top left corner is centered on the world origin. The image will repeat if terrain generates beyond its size.

Note: values in the image are read using `get_pixel` and are assumed to be between 0 and 1 (normalized). These values will be transformed by [VoxelGeneratorHeightmap.height_start](VoxelGeneratorHeightmap.md#i_height_start) and [VoxelGeneratorHeightmap.height_range](VoxelGeneratorHeightmap.md#i_height_range).

## Properties: 


Type                                                                      | Name                             | Default 
------------------------------------------------------------------------- | -------------------------------- | --------
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)    | [blur_enabled](#i_blur_enabled)  | false   
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [height_range](#i_height_range)  | 200.0   
[Image](https://docs.godotengine.org/en/stable/classes/class_image.html)  | [image](#i_image)                |         
<p></p>

## Property Descriptions

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_blur_enabled"></span> **blur_enabled** = false

*(This property has no documentation)*

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_height_range"></span> **height_range** = 200.0


### [Image](https://docs.godotengine.org/en/stable/classes/class_image.html)<span id="i_image"></span> **image**

Sets the image that will be used as a heightmap. Only the red channel will be used. It is preferable to use an image using the `RF` or `RH` format, which contain higher resolution heights. Common images only have 8-bit depth and will appear blocky.

_Generated on Aug 09, 2025_
