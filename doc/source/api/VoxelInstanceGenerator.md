# VoxelInstanceGenerator

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)

Decides where to spawn instances on top of a voxel surface.

## Description: 

Generates the necessary information to spawn instances on top of a voxel surface. This may be used by a [VoxelInstancer](VoxelInstancer.md).

Note: to generate voxels, see [VoxelGenerator](VoxelGenerator.md).

## Properties: 


Type                                                                                            | Name                                                                               | Default                    
----------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------- | ---------------------------
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                        | [density](#i_density)                                                              | 0.1                        
[EmitMode](VoxelInstanceGenerator.md#enumerations)                                              | [emit_mode](#i_emit_mode)                                                          | EMIT_FROM_VERTICES (0)     
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                        | [jitter](#i_jitter)                                                                | 1.0                        
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                        | [max_height](#i_max_height)                                                        | 3.4028235e+38              
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                        | [max_height_falloff](#i_max_height_falloff)                                        | 0.0                        
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                        | [max_scale](#i_max_scale)                                                          | 1.0                        
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                        | [max_slope_degrees](#i_max_slope_degrees)                                          | 180.0                      
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                        | [max_slope_falloff_degrees](#i_max_slope_falloff_degrees)                          | 0.0                        
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                        | [min_height](#i_min_height)                                                        | 1.1754944e-38              
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                        | [min_height_falloff](#i_min_height_falloff)                                        | 0.0                        
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                        | [min_scale](#i_min_scale)                                                          | 1.0                        
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                        | [min_slope_degrees](#i_min_slope_degrees)                                          | 0.0                        
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                        | [min_slope_falloff_degrees](#i_min_slope_falloff_degrees)                          | 0.0                        
[Noise](https://docs.godotengine.org/en/stable/classes/class_noise.html)                        | [noise](#i_noise)                                                                  |                            
[Dimension](VoxelInstanceGenerator.md#enumerations)                                             | [noise_dimension](#i_noise_dimension)                                              | DIMENSION_3D (1)           
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                        | [noise_falloff](#i_noise_falloff)                                                  | 0.0                        
[VoxelGraphFunction](VoxelGraphFunction.md)                                                     | [noise_graph](#i_noise_graph)                                                      |                            
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                        | [noise_on_scale](#i_noise_on_scale)                                                | 0.0                        
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                        | [noise_threshold](#i_noise_threshold)                                              | 0.0                        
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                        | [offset_along_normal](#i_offset_along_normal)                                      | 0.0                        
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)                          | [random_rotation](#i_random_rotation)                                              | true                       
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)                          | [random_vertical_flip](#i_random_vertical_flip)                                    | false                      
[Distribution](VoxelInstanceGenerator.md#enumerations)                                          | [scale_distribution](#i_scale_distribution)                                        | DISTRIBUTION_QUADRATIC (1) 
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)                          | [snap_to_generator_sdf_enabled](#i_snap_to_generator_sdf_enabled)                  | false                      
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                            | [snap_to_generator_sdf_sample_count](#i_snap_to_generator_sdf_sample_count)        | 2                          
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                        | [snap_to_generator_sdf_search_distance](#i_snap_to_generator_sdf_search_distance)  | 1.0                        
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                        | [triangle_area_threshold](#i_triangle_area_threshold)                              | 0.0                        
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                        | [vertical_alignment](#i_vertical_alignment)                                        | 1.0                        
[PackedInt32Array](https://docs.godotengine.org/en/stable/classes/class_packedint32array.html)  | [voxel_texture_filter_array](#i_voxel_texture_filter_array)                        | PackedInt32Array(0)        
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)                          | [voxel_texture_filter_enabled](#i_voxel_texture_filter_enabled)                    | false                      
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                        | [voxel_texture_filter_threshold](#i_voxel_texture_filter_threshold)                | 0.5                        
<p></p>

## Methods: 


Return                                                                | Signature                                                                                                                                        
--------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [get_voxel_texture_filter_mask](#i_get_voxel_texture_filter_mask) ( ) const                                                                      
[void](#)                                                             | [set_voxel_texture_filter_mask](#i_set_voxel_texture_filter_mask) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mask )  
<p></p>

## Enumerations: 

enum **EmitMode**: 

- <span id="i_EMIT_FROM_VERTICES"></span>**EMIT_FROM_VERTICES** = **0** --- Use vertices of the mesh to spawn instances. This is the fasted option, but can produce noticeable patterns.
- <span id="i_EMIT_FROM_FACES_FAST"></span>**EMIT_FROM_FACES_FAST** = **1** --- Uses faces of the mesh to spawn instances. It is a balanced option with some shortcuts taken, without causing too noticeable patterns.
- <span id="i_EMIT_FROM_FACES"></span>**EMIT_FROM_FACES** = **2** --- Uses faces of the mesh to spawn instances. This is the slowest option, but should produce no noticeable patterns.
- <span id="i_EMIT_ONE_PER_TRIANGLE"></span>**EMIT_ONE_PER_TRIANGLE** = **3** --- Uses faces of the mesh to spawn instances (where faces are triangles). Only one instance is spawned per triangle. By default, it spawns in the middle of triangles. Randomness can be added to this position with [jitter](VoxelInstanceGenerator.md#i_jitter).
- <span id="i_EMIT_MODE_COUNT"></span>**EMIT_MODE_COUNT** = **4**

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

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_density"></span> **density** = 0.1

Controls how many instances are generated. Might give different results depending on the type of emission chosen.

### [EmitMode](VoxelInstanceGenerator.md#enumerations)<span id="i_emit_mode"></span> **emit_mode** = EMIT_FROM_VERTICES (0)

In which way instances are primarily emitted.

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_jitter"></span> **jitter** = 1.0

Controls randomness of spawning position when [emit_mode](VoxelInstanceGenerator.md#i_emit_mode) is set to [EMIT_ONE_PER_TRIANGLE](VoxelInstanceGenerator.md#i_EMIT_ONE_PER_TRIANGLE).

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_max_height"></span> **max_height** = 3.4028235e+38

Instances will not be created above this height.

This also depends on the chosen [VoxelInstancer.up_mode](VoxelInstancer.md#i_up_mode).

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_max_height_falloff"></span> **max_height_falloff** = 0.0

Distance over which density will fade, when below [max_height](VoxelInstanceGenerator.md#i_max_height).

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_max_scale"></span> **max_scale** = 1.0

Minimum scale instances will be randomized with.

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_max_slope_degrees"></span> **max_slope_degrees** = 180.0

Instances will not spawn if the ground has a slope higher than this angle.

This also depends on the chosen [VoxelInstancer.up_mode](VoxelInstancer.md#i_up_mode).

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_max_slope_falloff_degrees"></span> **max_slope_falloff_degrees** = 0.0

Angular distance over which density will fade, when below [max_slope_degrees](VoxelInstanceGenerator.md#i_max_slope_degrees).

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_min_height"></span> **min_height** = 1.1754944e-38

Instances will not be created below this height. This also depends on the chosen [VoxelInstancer.up_mode](VoxelInstancer.md#i_up_mode).

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_min_height_falloff"></span> **min_height_falloff** = 0.0

Distance over which density will fade, when above [min_height](VoxelInstanceGenerator.md#i_min_height).

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_min_scale"></span> **min_scale** = 1.0

Maximum scale instances will be randomized with.

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_min_slope_degrees"></span> **min_slope_degrees** = 0.0

Instances will not spawn if the ground has a slope lower than this angle.

This also depends on the chosen [VoxelInstancer.up_mode](VoxelInstancer.md#i_up_mode).

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_min_slope_falloff_degrees"></span> **min_slope_falloff_degrees** = 0.0

Angular distance over which density will fade, when above [min_slope_degrees](VoxelInstanceGenerator.md#i_min_slope_degrees).

### [Noise](https://docs.godotengine.org/en/stable/classes/class_noise.html)<span id="i_noise"></span> **noise**

Noise used to filter out spawned instances, so that they may spawn in patterns described by the noise.

### [Dimension](VoxelInstanceGenerator.md#enumerations)<span id="i_noise_dimension"></span> **noise_dimension** = DIMENSION_3D (1)

Which dimension should be used when evaluating [noise](VoxelInstanceGenerator.md#i_noise) and [noise_graph](VoxelInstanceGenerator.md#i_noise_graph).

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_noise_falloff"></span> **noise_falloff** = 0.0

Noise range over which density will fade. For example if falloff is 0.3, then density will fade when noise values are between 0 and 0.3.

### [VoxelGraphFunction](VoxelGraphFunction.md)<span id="i_noise_graph"></span> **noise_graph**

Graph function used to filter out spawned instances, similar to [noise](VoxelInstanceGenerator.md#i_noise), but allows more custom noise computations.

The graph must have 2 inputs (X and Z) if [noise_dimension](VoxelInstanceGenerator.md#i_noise_dimension) is 2D, and 3 inputs (X, Y and Z) if 3D. There must be one SDF output.

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_noise_on_scale"></span> **noise_on_scale** = 0.0

How much [noise](VoxelInstanceGenerator.md#i_noise) also affects the scale of instances.

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_noise_threshold"></span> **noise_threshold** = 0.0

Expands or shrink noise filtering. Higher values expand the areas where instances will spawn, lower values shrinks them.

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_offset_along_normal"></span> **offset_along_normal** = 0.0

Offsets spawned instances along the normal of the ground.

The normal depends on [VoxelInstancer.up_mode](VoxelInstancer.md#i_up_mode) and is also affected by [vertical_alignment](VoxelInstanceGenerator.md#i_vertical_alignment).

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_random_rotation"></span> **random_rotation** = true

When enbabled, instances will be given a random rotation. If not, they will use a consistent rotation depending on the ground slope.

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_random_vertical_flip"></span> **random_vertical_flip** = false

When enabled, instances will randomly be flipped upside down. This can be useful with small rocks to create illusion of more variety.

### [Distribution](VoxelInstanceGenerator.md#enumerations)<span id="i_scale_distribution"></span> **scale_distribution** = DISTRIBUTION_QUADRATIC (1)

Sets how random scales are distributed.

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_snap_to_generator_sdf_enabled"></span> **snap_to_generator_sdf_enabled** = false

Enables snapping to generator SDF. The generator's SDF values will be queried to move instances along their normal to be closerr to ground. Can help reduce the occurrence of "floating" or "buried" instances when moving close to them. This requires the terrain's generator to support series generation. Side-effects: instances might become floating or buried when seen from far away; spawning might become less even.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_snap_to_generator_sdf_sample_count"></span> **snap_to_generator_sdf_sample_count** = 2

How many samples per instance will be taken from the generator in order to approach the snapping position. More samples increases precision, but is more expensive.

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_snap_to_generator_sdf_search_distance"></span> **snap_to_generator_sdf_search_distance** = 1.0

Distance up and down across which snapping will search for ground location.

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_triangle_area_threshold"></span> **triangle_area_threshold** = 0.0

If set greater than zero, triangles of the ground with an area lower than this threshold will be ignored.

Some meshing algorithms can often produce thin or small triangles that can affect distribution quality of spawned instances. If this happens, use this property to filter them out.

Note: this property is relative to LOD0. The generator will scale it when spawning instances on meshes of different LOD index.

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_vertical_alignment"></span> **vertical_alignment** = 1.0

Sets how much instances will align with the ground.

If 0, they will completely align with the ground.

If 1, they will completely align with whichever direction is considered "up".

This depends on [VoxelInstancer.up_mode](VoxelInstancer.md#i_up_mode).

### [PackedInt32Array](https://docs.godotengine.org/en/stable/classes/class_packedint32array.html)<span id="i_voxel_texture_filter_array"></span> **voxel_texture_filter_array** = PackedInt32Array(0)

Specifies which voxel texture indices on top of which instances may spawn.

This only works when [voxel_texture_filter_enabled](VoxelInstanceGenerator.md#i_voxel_texture_filter_enabled) is enabled, and [VoxelMesherTransvoxel](VoxelMesherTransvoxel.md) is used with [VoxelMesherTransvoxel.texturing_mode](VoxelMesherTransvoxel.md#i_texturing_mode) set to [VoxelMesherTransvoxel.TEXTURES_BLEND_4_OVER_16](VoxelMesherTransvoxel.md#i_TEXTURES_BLEND_4_OVER_16).

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_voxel_texture_filter_enabled"></span> **voxel_texture_filter_enabled** = false

When true, enables filtering of instances based on voxel texture indices. See [voxel_texture_filter_array](VoxelInstanceGenerator.md#i_voxel_texture_filter_array).

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_voxel_texture_filter_threshold"></span> **voxel_texture_filter_threshold** = 0.5

When [voxel_texture_filter_enabled](VoxelInstanceGenerator.md#i_voxel_texture_filter_enabled) is active, controls how much of filtered texture has to be present for instances to spawn. The value must be betweem 0 and 1.

## Method Descriptions

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_voxel_texture_filter_mask"></span> **get_voxel_texture_filter_mask**( ) 

*(This method has no documentation)*

### [void](#)<span id="i_set_voxel_texture_filter_mask"></span> **set_voxel_texture_filter_mask**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mask ) 

*(This method has no documentation)*

_Generated on Aug 09, 2025_
