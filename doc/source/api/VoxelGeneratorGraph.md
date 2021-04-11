# VoxelGeneratorGraph

Inherits: [VoxelGenerator](VoxelGenerator.md)


Graph-based generator for smooth voxel worlds.

## Description: 

Generates SDF voxel data from a graph of operations.

Warning: methods to modify the graph should only be called from the main thread.

## Methods: 


Return                                                                                  | Signature                                                                                                                                                                                                                                                                                                                                                                                   
--------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                                               | [add_connection](#i_add_connection) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_port_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_port_index )        
[void](#)                                                                               | [bake_sphere_bumpmap](#i_bake_sphere_bumpmap) ( [Image](https://docs.godotengine.org/en/stable/classes/class_image.html) im, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) ref_radius, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) sdf_min, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) sdf_max )      
[void](#)                                                                               | [bake_sphere_normalmap](#i_bake_sphere_normalmap) ( [Image](https://docs.godotengine.org/en/stable/classes/class_image.html) im, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) ref_radius, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) strength )                                                                                   
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)                  | [can_connect](#i_can_connect) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_port_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_port_index ) const        
[void](#)                                                                               | [clear](#i_clear) ( )                                                                                                                                                                                                                                                                                                                                                                       
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)      | [compile](#i_compile) ( )                                                                                                                                                                                                                                                                                                                                                                   
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                    | [create_node](#i_create_node) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) type_id, [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) position, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) id=0 )                                                                                                            
[void](#)                                                                               | [debug_load_waves_preset](#i_debug_load_waves_preset) ( )                                                                                                                                                                                                                                                                                                                                   
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                | [debug_measure_microseconds_per_voxel](#i_debug_measure_microseconds_per_voxel) ( [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) use_singular_queries )                                                                                                                                                                                                             
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                    | [find_node_by_name](#i_find_node_by_name) ( [String](https://docs.godotengine.org/en/stable/classes/class_string.html) name ) const                                                                                                                                                                                                                                                         
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                | [generate_single](#i_generate_single) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) arg0 )                                                                                                                                                                                                                                                                 
[Array](https://docs.godotengine.org/en/stable/classes/class_array.html)                | [get_connections](#i_get_connections) ( ) const                                                                                                                                                                                                                                                                                                                                             
[Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)            | [get_node_default_input](#i_get_node_default_input) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) input_index ) const                                                                                                                                                                
[Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html)            | [get_node_gui_position](#i_get_node_gui_position) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id ) const                                                                                                                                                                                                                                                    
[PoolIntArray](https://docs.godotengine.org/en/stable/classes/class_poolintarray.html)  | [get_node_ids](#i_get_node_ids) ( ) const                                                                                                                                                                                                                                                                                                                                                   
[Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)            | [get_node_param](#i_get_node_param) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) param_index ) const                                                                                                                                                                                
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                    | [get_node_type_count](#i_get_node_type_count) ( ) const                                                                                                                                                                                                                                                                                                                                     
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                    | [get_node_type_id](#i_get_node_type_id) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id ) const                                                                                                                                                                                                                                                              
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)      | [get_node_type_info](#i_get_node_type_info) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) type_id ) const                                                                                                                                                                                                                                                          
[void](#)                                                                               | [remove_connection](#i_remove_connection) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_port_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_port_index )  
[void](#)                                                                               | [remove_node](#i_remove_node) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id )                                                                                                                                                                                                                                                                              
[void](#)                                                                               | [set_node_default_input](#i_set_node_default_input) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) input_index, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) value )                                                                                  
[void](#)                                                                               | [set_node_gui_position](#i_set_node_gui_position) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) position )                                                                                                                                                                   
[void](#)                                                                               | [set_node_param](#i_set_node_param) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) param_index, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) value )                                                                                                  
[void](#)                                                                               | [set_node_param_null](#i_set_node_param_null) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) param_index )                                                                                                                                                                            
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
- **NODE_MIX** = **21**
- **NODE_REMAP** = **22**
- **NODE_SMOOTHSTEP** = **23**
- **NODE_CURVE** = **24**
- **NODE_SELECT** = **25**
- **NODE_NOISE_2D** = **26**
- **NODE_NOISE_3D** = **27**
- **NODE_IMAGE_2D** = **28**
- **NODE_SDF_PLANE** = **29**
- **NODE_SDF_BOX** = **30**
- **NODE_SDF_SPHERE** = **31**
- **NODE_SDF_TORUS** = **32**
- **NODE_SDF_PREVIEW** = **33**
- **NODE_NORMALIZE_3D** = **35**
- **NODE_FAST_NOISE_2D** = **36**
- **NODE_FAST_NOISE_3D** = **37**
- **NODE_FAST_NOISE_GRADIENT_2D** = **38**
- **NODE_FAST_NOISE_GRADIENT_3D** = **39**
- **NODE_TYPE_COUNT** = **40**


## Method Descriptions

- [void](#)<span id="i_add_connection"></span> **add_connection**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_port_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_port_index ) 


- [void](#)<span id="i_bake_sphere_bumpmap"></span> **bake_sphere_bumpmap**( [Image](https://docs.godotengine.org/en/stable/classes/class_image.html) im, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) ref_radius, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) sdf_min, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) sdf_max ) 


- [void](#)<span id="i_bake_sphere_normalmap"></span> **bake_sphere_normalmap**( [Image](https://docs.godotengine.org/en/stable/classes/class_image.html) im, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) ref_radius, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) strength ) 


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_can_connect"></span> **can_connect**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_port_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_port_index ) 


- [void](#)<span id="i_clear"></span> **clear**( ) 


- [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)<span id="i_compile"></span> **compile**( ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_create_node"></span> **create_node**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) type_id, [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) position, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) id=0 ) 


- [void](#)<span id="i_debug_load_waves_preset"></span> **debug_load_waves_preset**( ) 


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_debug_measure_microseconds_per_voxel"></span> **debug_measure_microseconds_per_voxel**( [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) use_singular_queries ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_find_node_by_name"></span> **find_node_by_name**( [String](https://docs.godotengine.org/en/stable/classes/class_string.html) name ) 


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_generate_single"></span> **generate_single**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) arg0 ) 


- [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_get_connections"></span> **get_connections**( ) 


- [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)<span id="i_get_node_default_input"></span> **get_node_default_input**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) input_index ) 


- [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html)<span id="i_get_node_gui_position"></span> **get_node_gui_position**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id ) 


- [PoolIntArray](https://docs.godotengine.org/en/stable/classes/class_poolintarray.html)<span id="i_get_node_ids"></span> **get_node_ids**( ) 


- [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)<span id="i_get_node_param"></span> **get_node_param**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) param_index ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_node_type_count"></span> **get_node_type_count**( ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_node_type_id"></span> **get_node_type_id**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id ) 


- [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)<span id="i_get_node_type_info"></span> **get_node_type_info**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) type_id ) 


- [void](#)<span id="i_remove_connection"></span> **remove_connection**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_port_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) dst_port_index ) 


- [void](#)<span id="i_remove_node"></span> **remove_node**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id ) 


- [void](#)<span id="i_set_node_default_input"></span> **set_node_default_input**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) input_index, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) value ) 


- [void](#)<span id="i_set_node_gui_position"></span> **set_node_gui_position**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) position ) 


- [void](#)<span id="i_set_node_param"></span> **set_node_param**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) param_index, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) value ) 


- [void](#)<span id="i_set_node_param_null"></span> **set_node_param_null**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) node_id, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) param_index ) 


_Generated on Apr 10, 2021_
