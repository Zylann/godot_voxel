# VoxelInstanceGenerator

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)

Decides where to spawn instances on top of a voxel surface.

## Description: 

Generates the necessary information to spawn instances on top of a voxel surface. This may be used by a [VoxelInstancer](VoxelInstancer.md).

Note: to generate voxels, see [VoxelGenerator](VoxelGenerator.md).

## Properties: 


Type                  | Name                                             | Default     
--------------------- | ------------------------------------------------ | ------------
`float`               | [density](#i_density)                            | 0.1         
`int`                 | [emit_mode](#i_emit_mode)                        | 0           
`float`               | [max_height](#i_max_height)                      | 3.40282e+38 
`float`               | [max_scale](#i_max_scale)                        | 1.0         
`float`               | [max_slope_degrees](#i_max_slope_degrees)        | 180.0       
`float`               | [min_height](#i_min_height)                      | 1.17549e-38 
`float`               | [min_scale](#i_min_scale)                        | 1.0         
`float`               | [min_slope_degrees](#i_min_slope_degrees)        | 0.0         
`Noise`               | [noise](#i_noise)                                |             
`int`                 | [noise_dimension](#i_noise_dimension)            | 1           
`VoxelGraphFunction`  | [noise_graph](#i_noise_graph)                    |             
`float`               | [noise_on_scale](#i_noise_on_scale)              | 0.0         
`float`               | [offset_along_normal](#i_offset_along_normal)    | 0.0         
`bool`                | [random_rotation](#i_random_rotation)            | true        
`bool`                | [random_vertical_flip](#i_random_vertical_flip)  | false       
`int`                 | [scale_distribution](#i_scale_distribution)      | 1           
`float`               | [vertical_alignment](#i_vertical_alignment)      | 1.0         
<p></p>

## Enumerations: 

enum **EmitMode**: 

- <span id="i_EMIT_FROM_VERTICES"></span>**EMIT_FROM_VERTICES** = **0** --- Use vertices of the mesh to spawn instances. This is the fasted option, but can produce noticeable patterns.
- <span id="i_EMIT_FROM_FACES_FAST"></span>**EMIT_FROM_FACES_FAST** = **1** --- Uses faces of the mesh to spawn instances. It is a balanced option with some shortcuts taken, without causing too noticeable patterns.
- <span id="i_EMIT_FROM_FACES"></span>**EMIT_FROM_FACES** = **2** --- Uses faces of the mesh to spawn instances. This is the slowest option, but should produce no noticeable patterns.
- <span id="i_EMIT_MODE_COUNT"></span>**EMIT_MODE_COUNT** = **3**

enum **Distribution**: 

- <span id="i_DISTRIBUTION_LINEAR"></span>**DISTRIBUTION_LINEAR** = **0** --- Uniform distribution.
- <span id="i_DISTRIBUTION_QUADRATIC"></span>**DISTRIBUTION_QUADRATIC** = **1** --- Distribution with more small items, and fewer big ones.
- <span id="i_DISTRIBUTION_CUBIC"></span>**DISTRIBUTION_CUBIC** = **2** --- Distribution with even more small items, and even fewer big ones.
- <span id="i_DISTRIBUTION_QUINTIC"></span>**DISTRIBUTION_QUINTIC** = **3**
- <span id="i_DISTRIBUTION_COUNT"></span>**DISTRIBUTION_COUNT** = **4**

enum **Dimension**: 

- <span id="i_DIMENSION_2D"></span>**DIMENSION_2D** = **0**
- <span id="i_DIMENSION_3D"></span>**DIMENSION_3D** = **1**
- <span id="i_DIMENSION_COUNT"></span>**DIMENSION_COUNT** = **2**


## Property Descriptions

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_density"></span> **density** = 0.1

Controls how many instances are generated. Might give different results depending on the type of emission chosen.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_emit_mode"></span> **emit_mode** = 0

In which way instances are primarily emitted.

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_max_height"></span> **max_height** = 3.40282e+38

Instances will not be created above this height.

This also depends on the chosen [VoxelInstancer.up_mode](VoxelInstancer.md#i_up_mode).

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_max_scale"></span> **max_scale** = 1.0

Minimum scale instances will be randomized with.

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_max_slope_degrees"></span> **max_slope_degrees** = 180.0

Instances will not spawn if the ground has a slope higher than this angle.

This also depends on the chosen [VoxelInstancer.up_mode](VoxelInstancer.md#i_up_mode).

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_min_height"></span> **min_height** = 1.17549e-38

Instances will not be created below this height. This also depends on the chosen [VoxelInstancer.up_mode](VoxelInstancer.md#i_up_mode).

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_min_scale"></span> **min_scale** = 1.0

Maximum scale instances will be randomized with.

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_min_slope_degrees"></span> **min_slope_degrees** = 0.0

Instances will not spawn if the ground has a slope lower than this angle.

This also depends on the chosen [VoxelInstancer.up_mode](VoxelInstancer.md#i_up_mode).

- [Noise](https://docs.godotengine.org/en/stable/classes/class_noise.html)<span id="i_noise"></span> **noise**

Noise used to filter out spawned instances, so that they may spawn in patterns described by the noise.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_noise_dimension"></span> **noise_dimension** = 1

Which dimension should be used when evaluating [VoxelInstanceGenerator.noise](VoxelInstanceGenerator.md#i_noise) and [VoxelInstanceGenerator.noise_graph](VoxelInstanceGenerator.md#i_noise_graph).

- [VoxelGraphFunction](VoxelGraphFunction.md)<span id="i_noise_graph"></span> **noise_graph**

Graph function used to filter out spawned instances, similar to [VoxelInstanceGenerator.noise](VoxelInstanceGenerator.md#i_noise), but allows more custom noise computations.

The graph must have 2 inputs (X and Z) if [VoxelInstanceGenerator.noise_dimension](VoxelInstanceGenerator.md#i_noise_dimension) is 2D, and 3 inputs (X, Y and Z) if 3D. There must be one SDF output.

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_noise_on_scale"></span> **noise_on_scale** = 0.0

How much [VoxelInstanceGenerator.noise](VoxelInstanceGenerator.md#i_noise) also affects the scale of instances.

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_offset_along_normal"></span> **offset_along_normal** = 0.0

Offsets spawned instances along the normal of the ground.

The normal depends on [VoxelInstancer.up_mode](VoxelInstancer.md#i_up_mode) and is also affected by [VoxelInstanceGenerator.vertical_alignment](VoxelInstanceGenerator.md#i_vertical_alignment).

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_random_rotation"></span> **random_rotation** = true

When enbabled, instances will be given a random rotation. If not, they will use a consistent rotation depending on the ground slope.

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_random_vertical_flip"></span> **random_vertical_flip** = false

When enabled, instances will randomly be flipped upside down. This can be useful with small rocks to create illusion of more variety.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_scale_distribution"></span> **scale_distribution** = 1

Sets how random scales are distributed.

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_vertical_alignment"></span> **vertical_alignment** = 1.0

Sets how much instances will align with the ground.

If 0, they will completely align with the ground.

If 1, they will completely align with whichever direction is considered "up".

This depends on [VoxelInstancer.up_mode](VoxelInstancer.md#i_up_mode).

_Generated on Nov 11, 2023_
