# VoxelGeneratorGraph

Inherits: [VoxelGenerator](VoxelGenerator.md)


Graph-based voxel generator.

## Description: 

Generates voxel data from a graph of per-voxel operations.

The graph must be created, compiled, and only then blocks can be generated.

It can be used with SDF-based smooth terrain, and also blocky terrains.

Warning: methods to modify the graph should only be called from the main thread.

## Properties: 


Type     | Name                                                           | Default 
-------- | -------------------------------------------------------------- | --------
`bool`   | [debug_block_clipping](#i_debug_block_clipping)                | false   
`float`  | [sdf_clip_threshold](#i_sdf_clip_threshold)                    | 1.5     
`int`    | [subdivision_size](#i_subdivision_size)                        | 16      
`bool`   | [use_optimized_execution_map](#i_use_optimized_execution_map)  | true    
`bool`   | [use_subdivision](#i_use_subdivision)                          | true    
`bool`   | [use_xz_caching](#i_use_xz_caching)                            | true    
<p></p>

## Methods: 


Return                                                                              | Signature                                                                                                                                                                                                                                                                                                                                                                               
----------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                                           | [bake_sphere_bumpmap](#i_bake_sphere_bumpmap) ( [Image](https://docs.godotengine.org/en/stable/classes/class_image.html) im, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) ref_radius, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) sdf_min, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) sdf_max )  
[void](#)                                                                           | [bake_sphere_normalmap](#i_bake_sphere_normalmap) ( [Image](https://docs.godotengine.org/en/stable/classes/class_image.html) im, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) ref_radius, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) strength )                                                                               
[void](#)                                                                           | [clear](#i_clear) ( )                                                                                                                                                                                                                                                                                                                                                                   
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)  | [compile](#i_compile) ( )                                                                                                                                                                                                                                                                                                                                                               
[Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html)        | [debug_analyze_range](#i_debug_analyze_range) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) min_pos, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) max_pos ) const                                                                                                                                                      
[void](#)                                                                           | [debug_load_waves_preset](#i_debug_load_waves_preset) ( )                                                                                                                                                                                                                                                                                                                               
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)            | [debug_measure_microseconds_per_voxel](#i_debug_measure_microseconds_per_voxel) ( [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) use_singular_queries )                                                                                                                                                                                                         
[VoxelGraphFunction](VoxelGraphFunction.md)                                         | [get_main_function](#i_get_main_function) ( ) const                                                                                                                                                                                                                                                                                                                                     
<p></p>

## Signals: 

- node_name_changed( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id ) 

## Property Descriptions

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_debug_block_clipping"></span> **debug_block_clipping** = false

When enabled, if the graph outputs SDF data, generated blocks that would otherwise be clipped will be inverted. This has the effect of them showing up as "walls artifacts", which is useful to visualize where the optimization occurs.

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_sdf_clip_threshold"></span> **sdf_clip_threshold** = 1.5

When generating SDF blocks for a terrain, if the range analysis of a block is beyond this threshold, its SDF data will be considered either fully 1, or fully -1. This optimizes memory and processing time.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_subdivision_size"></span> **subdivision_size** = 16

When generating SDF blocks for a terrain, and if block size is divisible by this value, range analysis will operate on such subdivision. This allows to optimize away more precise areas. However, it may not be set too small otherwise overhead will outweight the benefits.

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_use_optimized_execution_map"></span> **use_optimized_execution_map** = true

If enabled, when generating blocks for a terrain, the generator will attempt to skip specific nodes if they are found to have no importance in specific areas.

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_use_subdivision"></span> **use_subdivision** = true

If enabled, member subdivision_size will be used.

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_use_xz_caching"></span> **use_xz_caching** = true

If enabled, the generator will run only once branches of the graph that only depend on X and Z. This is effective when part of the graph generates a heightmap, as this part is not volumetric.

## Method Descriptions

- [void](#)<span id="i_bake_sphere_bumpmap"></span> **bake_sphere_bumpmap**( [Image](https://docs.godotengine.org/en/stable/classes/class_image.html) im, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) ref_radius, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) sdf_min, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) sdf_max ) 

Bakes a spherical bumpmap (or heightmap) using SDF output produced by the generator, if any. The bumpmap uses a panorama projection.

`ref_radius`: radius of the sphere on which heights will be sampled.

`strength`: strength of produced normals, may default to 1.0.

- [void](#)<span id="i_bake_sphere_normalmap"></span> **bake_sphere_normalmap**( [Image](https://docs.godotengine.org/en/stable/classes/class_image.html) im, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) ref_radius, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) strength ) 

Bakes a spherical normalmap using SDF output produced by the generator, if any. The normalmap uses a panorama projection. It is assumed the generator produces a spherical shape (like a planet). Such normalmap can be used to add more detail to distant views of a terrain using this generator.

`ref_radius`: radius of the sphere on which normals will be sampled.

`strength`: strength of produced normals, may default to 1.0.

Note: an alternative is to use distance normals feature with [VoxelLodTerrain](VoxelLodTerrain.md).

- [void](#)<span id="i_clear"></span> **clear**( ) 

Erases all nodes and connections from the graph.

- [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)<span id="i_compile"></span> **compile**( ) 

Compiles the graph so it can be used to generate blocks.

If it succeeds, the returned result is a dictionary with the following layout:

```gdscript
{
	"success": true
}

```

If it fails, the returned result may contain a message and the ID of a graph node that could be the cause:

```gdscript
{
	"success": false,
	"node_id": int,
	"message": String
}

```

The node ID will be -1 if the error is not about a particular node.

- [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html)<span id="i_debug_analyze_range"></span> **debug_analyze_range**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) min_pos, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) max_pos ) 


- [void](#)<span id="i_debug_load_waves_preset"></span> **debug_load_waves_preset**( ) 


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_debug_measure_microseconds_per_voxel"></span> **debug_measure_microseconds_per_voxel**( [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) use_singular_queries ) 


- [VoxelGraphFunction](VoxelGraphFunction.md)<span id="i_get_main_function"></span> **get_main_function**( ) 


_Generated on Mar 26, 2023_
