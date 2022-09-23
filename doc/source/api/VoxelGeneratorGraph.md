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


Return                                                                                          | Signature                                                                                                                                                                                                                                                                                                                                                                                   
----------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                                                       | [add_connection](#i_add_connection) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_port_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_port_index )        
[void](#)                                                                                       | [bake_sphere_bumpmap](#i_bake_sphere_bumpmap) ( [Image](https://docs.godotengine.org/en/stable/classes/class_image.html) im, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) ref_radius, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) sdf_min, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) sdf_max )      
[void](#)                                                                                       | [bake_sphere_normalmap](#i_bake_sphere_normalmap) ( [Image](https://docs.godotengine.org/en/stable/classes/class_image.html) im, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) ref_radius, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) strength )                                                                                   
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)                          | [can_connect](#i_can_connect) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_port_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_port_index ) const        
[void](#)                                                                                       | [clear](#i_clear) ( )                                                                                                                                                                                                                                                                                                                                                                       
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)              | [compile](#i_compile) ( )                                                                                                                                                                                                                                                                                                                                                                   
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                            | [create_node](#i_create_node) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) type_id, [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) position, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) id=0 )                                                                                                            
[Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html)                    | [debug_analyze_range](#i_debug_analyze_range) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) min_pos, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) max_pos ) const                                                                                                                                                          
[void](#)                                                                                       | [debug_load_waves_preset](#i_debug_load_waves_preset) ( )                                                                                                                                                                                                                                                                                                                                   
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                        | [debug_measure_microseconds_per_voxel](#i_debug_measure_microseconds_per_voxel) ( [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) use_singular_queries )                                                                                                                                                                                                             
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                            | [find_node_by_name](#i_find_node_by_name) ( [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html) name ) const                                                                                                                                                                                                                                                 
[String](https://docs.godotengine.org/en/stable/classes/class_string.html)                      | [generate_shader](#i_generate_shader) ( )                                                                                                                                                                                                                                                                                                                                                   
[Array](https://docs.godotengine.org/en/stable/classes/class_array.html)                        | [get_connections](#i_get_connections) ( ) const                                                                                                                                                                                                                                                                                                                                             
[Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)                    | [get_node_default_input](#i_get_node_default_input) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) input_index ) const                                                                                                                                                                
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)                          | [get_node_default_inputs_autoconnect](#i_get_node_default_inputs_autoconnect) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id ) const                                                                                                                                                                                                                        
[Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html)                    | [get_node_gui_position](#i_get_node_gui_position) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id ) const                                                                                                                                                                                                                                                    
[PackedInt32Array](https://docs.godotengine.org/en/stable/classes/class_packedint32array.html)  | [get_node_ids](#i_get_node_ids) ( ) const                                                                                                                                                                                                                                                                                                                                                   
[StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html)              | [get_node_name](#i_get_node_name) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id ) const                                                                                                                                                                                                                                                                    
[Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)                    | [get_node_param](#i_get_node_param) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) param_index ) const                                                                                                                                                                                
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                            | [get_node_type_count](#i_get_node_type_count) ( ) const                                                                                                                                                                                                                                                                                                                                     
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                            | [get_node_type_id](#i_get_node_type_id) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id ) const                                                                                                                                                                                                                                                              
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)              | [get_node_type_info](#i_get_node_type_info) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) type_id ) const                                                                                                                                                                                                                                                          
[void](#)                                                                                       | [remove_connection](#i_remove_connection) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_port_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_port_index )  
[void](#)                                                                                       | [remove_node](#i_remove_node) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id )                                                                                                                                                                                                                                                                              
[void](#)                                                                                       | [set_expression_node_inputs](#i_set_expression_node_inputs) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [PackedStringArray](https://docs.godotengine.org/en/stable/classes/class_packedstringarray.html) names )                                                                                                                                        
[void](#)                                                                                       | [set_node_default_input](#i_set_node_default_input) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) input_index, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) value )                                                                                  
[void](#)                                                                                       | [set_node_default_inputs_autoconnect](#i_set_node_default_inputs_autoconnect) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled )                                                                                                                                              
[void](#)                                                                                       | [set_node_gui_position](#i_set_node_gui_position) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) position )                                                                                                                                                                   
[void](#)                                                                                       | [set_node_name](#i_set_node_name) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html) name )                                                                                                                                                                                 
[void](#)                                                                                       | [set_node_param](#i_set_node_param) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) param_index, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) value )                                                                                                  
[void](#)                                                                                       | [set_node_param_null](#i_set_node_param_null) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) param_index )                                                                                                                                                                            
<p></p>

## Signals: 

- node_name_changed( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id ) 

## Enumerations: 

enum **NodeTypeID**: 

- **NODE_CONSTANT** = **0**
- **NODE_INPUT_X** = **1**
- **NODE_INPUT_Y** = **2**
- **NODE_INPUT_Z** = **3**
- **NODE_OUTPUT_SDF** = **4**
- **NODE_ADD** = **5**
- **NODE_SUBTRACT** = **6**
- **NODE_MULTIPLY** = **7**
- **NODE_DIVIDE** = **8**
- **NODE_SIN** = **9**
- **NODE_FLOOR** = **10**
- **NODE_ABS** = **11**
- **NODE_SQRT** = **12**
- **NODE_FRACT** = **13**
- **NODE_STEPIFY** = **14**
- **NODE_WRAP** = **15**
- **NODE_MIN** = **16**
- **NODE_MAX** = **17**
- **NODE_DISTANCE_2D** = **18**
- **NODE_DISTANCE_3D** = **19**
- **NODE_CLAMP** = **20**
- **NODE_MIX** = **22**
- **NODE_REMAP** = **23**
- **NODE_SMOOTHSTEP** = **24**
- **NODE_CURVE** = **25**
- **NODE_SELECT** = **26**
- **NODE_NOISE_2D** = **27**
- **NODE_NOISE_3D** = **28**
- **NODE_IMAGE_2D** = **29**
- **NODE_SDF_PLANE** = **30**
- **NODE_SDF_BOX** = **31**
- **NODE_SDF_SPHERE** = **32**
- **NODE_SDF_TORUS** = **33**
- **NODE_SDF_PREVIEW** = **34**
- **NODE_SDF_SPHERE_HEIGHTMAP** = **35**
- **NODE_SDF_SMOOTH_UNION** = **36**
- **NODE_SDF_SMOOTH_SUBTRACT** = **37**
- **NODE_NORMALIZE_3D** = **38**
- **NODE_FAST_NOISE_2D** = **39**
- **NODE_FAST_NOISE_3D** = **40**
- **NODE_FAST_NOISE_GRADIENT_2D** = **41**
- **NODE_FAST_NOISE_GRADIENT_3D** = **42**
- **NODE_OUTPUT_WEIGHT** = **43**
- **NODE_OUTPUT_SINGLE_TEXTURE** = **45**
- **NODE_EXPRESSION** = **46**
- **NODE_POWI** = **47**
- **NODE_POW** = **48**
- **NODE_INPUT_SDF** = **49**
- **NODE_TYPE_COUNT** = **50**


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

- [void](#)<span id="i_add_connection"></span> **add_connection**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_port_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_port_index ) 

Connects the outputs of a node to the input of another node. Connecting a node to itself, or in a way that can lead it back to itself, is not supported.

- [void](#)<span id="i_bake_sphere_bumpmap"></span> **bake_sphere_bumpmap**( [Image](https://docs.godotengine.org/en/stable/classes/class_image.html) im, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) ref_radius, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) sdf_min, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) sdf_max ) 

Bakes a spherical bumpmap (or heightmap) using SDF output produced by the generator, if any. The bumpmap uses a panorama projection.

`ref_radius`: radius of the sphere on which heights will be sampled.

`strength`: strength of produced normals, may default to 1.0.

- [void](#)<span id="i_bake_sphere_normalmap"></span> **bake_sphere_normalmap**( [Image](https://docs.godotengine.org/en/stable/classes/class_image.html) im, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) ref_radius, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) strength ) 

Bakes a spherical normalmap using SDF output produced by the generator, if any. The normalmap uses a panorama projection. It is assumed the generator produces a spherical shape (like a planet). Such normalmap can be used to add more detail to distant views of a terrain using this generator.

`ref_radius`: radius of the sphere on which normals will be sampled.

`strength`: strength of produced normals, may default to 1.0.

Note: an alternative is to use distance normals feature with [VoxelLodTerrain](VoxelLodTerrain.md).

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_can_connect"></span> **can_connect**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_port_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_port_index ) 

Tests if two ports can be connected together.

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

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_create_node"></span> **create_node**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) type_id, [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) position, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) id=0 ) 

Creates a graph node of a given type at a specific visual position. That position does not affect how the graph will perform, however it helps organizing nodes.

An optional ID can be specified. If left to 0, the ID will be generated.

This function then returns the ID of the node, which may be useful to modify other properties of the node later.

- [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html)<span id="i_debug_analyze_range"></span> **debug_analyze_range**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) min_pos, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) max_pos ) 


- [void](#)<span id="i_debug_load_waves_preset"></span> **debug_load_waves_preset**( ) 


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_debug_measure_microseconds_per_voxel"></span> **debug_measure_microseconds_per_voxel**( [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) use_singular_queries ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_find_node_by_name"></span> **find_node_by_name**( [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html) name ) 


- [String](https://docs.godotengine.org/en/stable/classes/class_string.html)<span id="i_generate_shader"></span> **generate_shader**( ) 


- [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_get_connections"></span> **get_connections**( ) 


- [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)<span id="i_get_node_default_input"></span> **get_node_default_input**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) input_index ) 


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_get_node_default_inputs_autoconnect"></span> **get_node_default_inputs_autoconnect**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id ) 


- [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html)<span id="i_get_node_gui_position"></span> **get_node_gui_position**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id ) 


- [PackedInt32Array](https://docs.godotengine.org/en/stable/classes/class_packedint32array.html)<span id="i_get_node_ids"></span> **get_node_ids**( ) 


- [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html)<span id="i_get_node_name"></span> **get_node_name**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id ) 


- [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)<span id="i_get_node_param"></span> **get_node_param**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) param_index ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_node_type_count"></span> **get_node_type_count**( ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_node_type_id"></span> **get_node_type_id**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id ) 


- [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)<span id="i_get_node_type_info"></span> **get_node_type_info**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) type_id ) 


- [void](#)<span id="i_remove_connection"></span> **remove_connection**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_port_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_port_index ) 

Removes an existing connection between two nodes of the graph.

- [void](#)<span id="i_remove_node"></span> **remove_node**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id ) 

Removes a node from the graph.

- [void](#)<span id="i_set_expression_node_inputs"></span> **set_expression_node_inputs**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [PackedStringArray](https://docs.godotengine.org/en/stable/classes/class_packedstringarray.html) names ) 

Configures inputs for an Expression node. `names` is the list of input names used in the expression.

If you create an Expression node from code, you may want to use this function, as inputs will not be setup automatically.

- [void](#)<span id="i_set_node_default_input"></span> **set_node_default_input**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) input_index, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) value ) 

Sets the value that will be used on an input of a node that has no inbound connection.

`value` may be a `float` for now.

- [void](#)<span id="i_set_node_default_inputs_autoconnect"></span> **set_node_default_inputs_autoconnect**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled ) 

Sets wether a node input with no inbound connection will automatically create a default connection when the graph is compiled.

This is only available on specific nodes. On other nodes, it has no effect.

- [void](#)<span id="i_set_node_gui_position"></span> **set_node_gui_position**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) position ) 

Sets the visual position of a node of the graph, as it will appear in the editor.

- [void](#)<span id="i_set_node_name"></span> **set_node_name**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html) name ) 

Sets a custom name for a node.

- [void](#)<span id="i_set_node_param"></span> **set_node_param**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) param_index, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) value ) 


- [void](#)<span id="i_set_node_param_null"></span> **set_node_param_null**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) param_index ) 


_Generated on Sep 10, 2022_
