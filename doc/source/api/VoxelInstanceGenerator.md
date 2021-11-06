# VoxelInstanceGenerator

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)


Decides where to spawn instances on top of a voxel surface.

## Description: 

Generates the necessry information to spawn instances on top of a voxel surface. This may be used by a [VoxelInstancer](VoxelInstancer.md).

Note: to generate voxels, see [VoxelGenerator](VoxelGenerator.md).

## Properties: 


Type             | Name                                             | Default     
---------------- | ------------------------------------------------ | ------------
`float`          | [density](#i_density)                            | 0.1         
`int`            | [emit_mode](#i_emit_mode)                        | 0           
`float`          | [max_height](#i_max_height)                      | 3.40282e+38 
`float`          | [max_scale](#i_max_scale)                        | 1.0         
`float`          | [max_slope_degrees](#i_max_slope_degrees)        | 180.0       
`float`          | [min_height](#i_min_height)                      | 1.17549e-38 
`float`          | [min_scale](#i_min_scale)                        | 1.0         
`float`          | [min_slope_degrees](#i_min_slope_degrees)        | 0.0         
`FastNoiseLite`  | [noise](#i_noise)                                |             
`int`            | [noise_dimension](#i_noise_dimension)            | 1           
`float`          | [noise_on_scale](#i_noise_on_scale)              | 0.0         
`float`          | [offset_along_normal](#i_offset_along_normal)    | 0.0         
`bool`           | [random_rotation](#i_random_rotation)            | true        
`bool`           | [random_vertical_flip](#i_random_vertical_flip)  | false       
`int`            | [scale_distribution](#i_scale_distribution)      | 1           
`float`          | [vertical_alignment](#i_vertical_alignment)      | 1.0         
<p></p>

## Enumerations: 

enum **EmitMode**: 

- **EMIT_FROM_VERTICES** = **0**
- **EMIT_FROM_FACES_FAST** = **1**
- **EMIT_FROM_FACES** = **2**
- **EMIT_MODE_COUNT** = **3**

enum **Distribution**: 

- **DISTRIBUTION_LINEAR** = **0**
- **DISTRIBUTION_QUADRATIC** = **1**
- **DISTRIBUTION_CUBIC** = **2**
- **DISTRIBUTION_QUINTIC** = **3**
- **DISTRIBUTION_COUNT** = **4**

enum **Dimension**: 

- **DIMENSION_2D** = **0**
- **DIMENSION_3D** = **1**
- **DIMENSION_COUNT** = **2**


## Property Descriptions

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_density"></span> **density** = 0.1


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_emit_mode"></span> **emit_mode** = 0


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_max_height"></span> **max_height** = 3.40282e+38


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_max_scale"></span> **max_scale** = 1.0


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_max_slope_degrees"></span> **max_slope_degrees** = 180.0


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_min_height"></span> **min_height** = 1.17549e-38


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_min_scale"></span> **min_scale** = 1.0


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_min_slope_degrees"></span> **min_slope_degrees** = 0.0


- [FastNoiseLite](FastNoiseLite.md)<span id="i_noise"></span> **noise**


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_noise_dimension"></span> **noise_dimension** = 1


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_noise_on_scale"></span> **noise_on_scale** = 0.0


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_offset_along_normal"></span> **offset_along_normal** = 0.0


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_random_rotation"></span> **random_rotation** = true


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_random_vertical_flip"></span> **random_vertical_flip** = false


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_scale_distribution"></span> **scale_distribution** = 1


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_vertical_alignment"></span> **vertical_alignment** = 1.0


_Generated on Nov 06, 2021_
