# VoxelGraphFunction

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)


Graph for generating or processing voxels.

## Description: 

Contains a graph that can be used to generate voxel data (when used as main function of a generator), or to be re-used into other graphs (like a sub-graph).

Currently this class only stores a graph, it cannot run actual processing on its own. To generate voxels with it, see [VoxelGeneratorGraph](VoxelGeneratorGraph.md).

## Properties: 


Type     | Name                                         | Default 
-------- | -------------------------------------------- | --------
`Array`  | [input_definitions](#i_input_definitions)    | []      
`Array`  | [output_definitions](#i_output_definitions)  | []      
<p></p>

## Methods: 


Return                                                                                          | Signature                                                                                                                                                                                                                                                                                                                                                                                   
----------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                                                       | [add_connection](#i_add_connection) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_port_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_port_index )        
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)                          | [can_connect](#i_can_connect) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_port_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_port_index ) const        
[void](#)                                                                                       | [clear](#i_clear) ( )                                                                                                                                                                                                                                                                                                                                                                       
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                            | [create_function_node](#i_create_function_node) ( [VoxelGraphFunction](VoxelGraphFunction.md) position, [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) _unnamed_arg2=0 )                                                                                                             
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                            | [create_node](#i_create_node) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) type_id, [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) position, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) id=0 )                                                                                                            
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                            | [find_node_by_name](#i_find_node_by_name) ( [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html) name ) const                                                                                                                                                                                                                                                 
[Array](https://docs.godotengine.org/en/stable/classes/class_array.html)                        | [get_connections](#i_get_connections) ( ) const                                                                                                                                                                                                                                                                                                                                             
[Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)                    | [get_node_default_input](#i_get_node_default_input) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) input_index ) const                                                                                                                                                                
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)                          | [get_node_default_inputs_autoconnect](#i_get_node_default_inputs_autoconnect) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id ) const                                                                                                                                                                                                                        
[Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html)                    | [get_node_gui_position](#i_get_node_gui_position) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id ) const                                                                                                                                                                                                                                                    
[Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html)                    | [get_node_gui_size](#i_get_node_gui_size) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id ) const                                                                                                                                                                                                                                                            
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
[void](#)                                                                                       | [set_node_gui_size](#i_set_node_gui_size) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) size )                                                                                                                                                                               
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
- **NODE_CUSTOM_INPUT** = **54**
- **NODE_CUSTOM_OUTPUT** = **55**
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
- **NODE_FAST_NOISE_2_2D** = **45**
- **NODE_FAST_NOISE_2_3D** = **46**
- **NODE_OUTPUT_SINGLE_TEXTURE** = **47**
- **NODE_EXPRESSION** = **48**
- **NODE_POWI** = **49**
- **NODE_POW** = **50**
- **NODE_INPUT_SDF** = **51**
- **NODE_COMMENT** = **52**
- **NODE_FUNCTION** = **53**
- **NODE_RELAY** = **56**
- **NODE_SPOTS_2D** = **57**
- **NODE_SPOTS_3D** = **58**
- **NODE_TYPE_COUNT** = **59**


## Property Descriptions

- [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_input_definitions"></span> **input_definitions** = []


- [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_output_definitions"></span> **output_definitions** = []


## Method Descriptions

- [void](#)<span id="i_add_connection"></span> **add_connection**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_port_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_port_index ) 

Connects the outputs of a node to the input of another node. Connecting a node to itself, or in a way that can lead it back to itself, is not supported.

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_can_connect"></span> **can_connect**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_port_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_port_index ) 

Tests if two ports can be connected together.

- [void](#)<span id="i_clear"></span> **clear**( ) 

Removes all nodes from the graph. Input and output definitions will not be cleared.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_create_function_node"></span> **create_function_node**( [VoxelGraphFunction](VoxelGraphFunction.md) position, [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) _unnamed_arg2=0 ) 

Creates a node based on an existing graph (creates a "sub-graph instance").

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_create_node"></span> **create_node**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) type_id, [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) position, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) id=0 ) 

Creates a graph node of a given type at a specific visual position.

The `position` parameter does not affect how the graph will perform, however it helps organizing nodes.

An optional ID can be specified. If left to 0, the ID will be generated.

This function then returns the ID of the node, which may be useful to modify other properties of the node later.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_find_node_by_name"></span> **find_node_by_name**( [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html) name ) 


- [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_get_connections"></span> **get_connections**( ) 


- [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)<span id="i_get_node_default_input"></span> **get_node_default_input**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) input_index ) 


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_get_node_default_inputs_autoconnect"></span> **get_node_default_inputs_autoconnect**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id ) 


- [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html)<span id="i_get_node_gui_position"></span> **get_node_gui_position**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id ) 


- [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html)<span id="i_get_node_gui_size"></span> **get_node_gui_size**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id ) 


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

`value` must be a `float` for now.

- [void](#)<span id="i_set_node_default_input"></span> **set_node_default_input**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) input_index, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) value ) 


- [void](#)<span id="i_set_node_default_inputs_autoconnect"></span> **set_node_default_inputs_autoconnect**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled ) 

Sets wether a node input with no inbound connection will automatically create a default connection when the graph is compiled.

This is only available on specific nodes. On other nodes, it has no effect.

- [void](#)<span id="i_set_node_gui_position"></span> **set_node_gui_position**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) position ) 

Sets the visual position of a node of the graph, as it will appear in the editor.

- [void](#)<span id="i_set_node_gui_size"></span> **set_node_gui_size**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) size ) 


- [void](#)<span id="i_set_node_name"></span> **set_node_name**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html) name ) 

Sets a custom name for a node.

- [void](#)<span id="i_set_node_param"></span> **set_node_param**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) param_index, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) value ) 


- [void](#)<span id="i_set_node_param_null"></span> **set_node_param_null**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) param_index ) 


_Generated on Mar 26, 2023_
