# FastNoise2

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)




## Properties: 


Type      | Name                                                         | Default 
--------- | ------------------------------------------------------------ | --------
`int`     | [cellular_distance_function](#i_cellular_distance_function)  | 0       
`float`   | [cellular_jitter](#i_cellular_jitter)                        | 1.0     
`int`     | [cellular_return_type](#i_cellular_return_type)              | 0       
`String`  | [encoded_node_tree](#i_encoded_node_tree)                    | ""      
`float`   | [fractal_gain](#i_fractal_gain)                              | 0.5     
`float`   | [fractal_lacunarity](#i_fractal_lacunarity)                  | 2.0     
`int`     | [fractal_octaves](#i_fractal_octaves)                        | 3       
`float`   | [fractal_ping_pong_strength](#i_fractal_ping_pong_strength)  | 2.0     
`int`     | [fractal_type](#i_fractal_type)                              | 0       
`int`     | [noise_type](#i_noise_type)                                  | 0       
`float`   | [period](#i_period)                                          | 64.0    
`bool`    | [remap_enabled](#i_remap_enabled)                            | false   
`float`   | [remap_input_max](#i_remap_input_max)                        | 1.0     
`float`   | [remap_input_min](#i_remap_input_min)                        | -1.0    
`float`   | [remap_output_max](#i_remap_output_max)                      | 1.0     
`float`   | [remap_output_min](#i_remap_output_min)                      | -1.0    
`int`     | [seed](#i_seed)                                              | 1337    
`bool`    | [terrace_enabled](#i_terrace_enabled)                        | false   
`float`   | [terrace_multiplier](#i_terrace_multiplier)                  | 1.0     
`float`   | [terrace_smoothness](#i_terrace_smoothness)                  | 0.0     
<p></p>

## Methods: 


Return                                                                      | Signature                                                                                                                                                                                                     
--------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                                   | [generate_image](#i_generate_image) ( [Image](https://docs.godotengine.org/en/stable/classes/class_image.html) image, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) tileable ) const 
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)    | [get_noise_2d_single](#i_get_noise_2d_single) ( [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) pos ) const                                                                      
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)    | [get_noise_3d_single](#i_get_noise_3d_single) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos ) const                                                                      
[String](https://docs.godotengine.org/en/stable/classes/class_string.html)  | [get_simd_level_name](#i_get_simd_level_name) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) level )                                                                                  
[void](#)                                                                   | [update_generator](#i_update_generator) ( )                                                                                                                                                                   
<p></p>

## Enumerations: 

enum **NoiseType**: 

- **TYPE_OPEN_SIMPLEX_2** = **0**
- **TYPE_SIMPLEX** = **1**
- **TYPE_PERLIN** = **2**
- **TYPE_VALUE** = **3**
- **TYPE_CELLULAR** = **4**
- **TYPE_ENCODED_NODE_TREE** = **5**

enum **FractalType**: 

- **FRACTAL_NONE** = **0**
- **FRACTAL_FBM** = **1**
- **FRACTAL_RIDGED** = **2**
- **FRACTAL_PING_PONG** = **3**

enum **CellularDistanceFunction**: 

- **CELLULAR_DISTANCE_EUCLIDEAN** = **0**
- **CELLULAR_DISTANCE_EUCLIDEAN_SQ** = **1**
- **CELLULAR_DISTANCE_MANHATTAN** = **2**
- **CELLULAR_DISTANCE_HYBRID** = **3**
- **CELLULAR_DISTANCE_MAX_AXIS** = **4**

enum **CellularReturnType**: 

- **CELLULAR_RETURN_INDEX_0** = **0**
- **CELLULAR_RETURN_INDEX_0_ADD_1** = **1**
- **CELLULAR_RETURN_INDEX_0_SUB_1** = **2**
- **CELLULAR_RETURN_INDEX_0_MUL_1** = **3**
- **CELLULAR_RETURN_INDEX_0_DIV_1** = **4**

enum **SIMDLevel**: 

- **SIMD_NULL** = **0**
- **SIMD_SCALAR** = **1**
- **SIMD_SSE** = **2**
- **SIMD_SSE2** = **4**
- **SIMD_SSE3** = **8**
- **SIMD_SSSE3** = **16**
- **SIMD_SSE41** = **32**
- **SIMD_SSE42** = **64**
- **SIMD_AVX** = **128**
- **SIMD_AVX2** = **256**
- **SIMD_AVX512** = **512**
- **SIMD_NEON** = **65536**


## Property Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_cellular_distance_function"></span> **cellular_distance_function** = 0


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_cellular_jitter"></span> **cellular_jitter** = 1.0


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_cellular_return_type"></span> **cellular_return_type** = 0


- [String](https://docs.godotengine.org/en/stable/classes/class_string.html)<span id="i_encoded_node_tree"></span> **encoded_node_tree** = ""


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_fractal_gain"></span> **fractal_gain** = 0.5


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_fractal_lacunarity"></span> **fractal_lacunarity** = 2.0


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_fractal_octaves"></span> **fractal_octaves** = 3


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_fractal_ping_pong_strength"></span> **fractal_ping_pong_strength** = 2.0


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_fractal_type"></span> **fractal_type** = 0


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_noise_type"></span> **noise_type** = 0


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_period"></span> **period** = 64.0


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_remap_enabled"></span> **remap_enabled** = false


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_remap_input_max"></span> **remap_input_max** = 1.0


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_remap_input_min"></span> **remap_input_min** = -1.0


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_remap_output_max"></span> **remap_output_max** = 1.0


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_remap_output_min"></span> **remap_output_min** = -1.0


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_seed"></span> **seed** = 1337


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_terrace_enabled"></span> **terrace_enabled** = false


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_terrace_multiplier"></span> **terrace_multiplier** = 1.0


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_terrace_smoothness"></span> **terrace_smoothness** = 0.0


## Method Descriptions

- [void](#)<span id="i_generate_image"></span> **generate_image**( [Image](https://docs.godotengine.org/en/stable/classes/class_image.html) image, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) tileable ) 


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_get_noise_2d_single"></span> **get_noise_2d_single**( [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) pos ) 


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_get_noise_3d_single"></span> **get_noise_3d_single**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos ) 


- [String](https://docs.godotengine.org/en/stable/classes/class_string.html)<span id="i_get_simd_level_name"></span> **get_simd_level_name**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) level ) 


- [void](#)<span id="i_update_generator"></span> **update_generator**( ) 


_Generated on Mar 26, 2023_
