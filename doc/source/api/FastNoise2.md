# FastNoise2

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)

Wrapper for the [FastNoise2](https://github.com/Auburn/FastNoise2) library.

## Description: 

Generates coherent noise with support for SIMD processing, which can speed up generation significantly compared to scalar implementations like [FastNoiseLite](https://docs.godotengine.org/en/stable/classes/class_fastnoiselite.html).

It uses a node-based structure internally for configuration, which is however not editable from within Godot at the moment. Instead, simplified properties are exposed. It possible to edit noise graphs with an external Noise Tool, which can export an encoded node tree as a base64 string.

The current integration uses FastNoise2 version 0.10.0-alpha. If you use encoded node trees made with the Noise Tool, it must match that version.

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

- <span id="i_TYPE_OPEN_SIMPLEX_2"></span>**TYPE_OPEN_SIMPLEX_2** = **0**
- <span id="i_TYPE_SIMPLEX"></span>**TYPE_SIMPLEX** = **1**
- <span id="i_TYPE_PERLIN"></span>**TYPE_PERLIN** = **2**
- <span id="i_TYPE_VALUE"></span>**TYPE_VALUE** = **3**
- <span id="i_TYPE_CELLULAR"></span>**TYPE_CELLULAR** = **4**
- <span id="i_TYPE_ENCODED_NODE_TREE"></span>**TYPE_ENCODED_NODE_TREE** = **5**

enum **FractalType**: 

- <span id="i_FRACTAL_NONE"></span>**FRACTAL_NONE** = **0**
- <span id="i_FRACTAL_FBM"></span>**FRACTAL_FBM** = **1**
- <span id="i_FRACTAL_RIDGED"></span>**FRACTAL_RIDGED** = **2**
- <span id="i_FRACTAL_PING_PONG"></span>**FRACTAL_PING_PONG** = **3**

enum **CellularDistanceFunction**: 

- <span id="i_CELLULAR_DISTANCE_EUCLIDEAN"></span>**CELLULAR_DISTANCE_EUCLIDEAN** = **0**
- <span id="i_CELLULAR_DISTANCE_EUCLIDEAN_SQ"></span>**CELLULAR_DISTANCE_EUCLIDEAN_SQ** = **1**
- <span id="i_CELLULAR_DISTANCE_MANHATTAN"></span>**CELLULAR_DISTANCE_MANHATTAN** = **2**
- <span id="i_CELLULAR_DISTANCE_HYBRID"></span>**CELLULAR_DISTANCE_HYBRID** = **3**
- <span id="i_CELLULAR_DISTANCE_MAX_AXIS"></span>**CELLULAR_DISTANCE_MAX_AXIS** = **4**

enum **CellularReturnType**: 

- <span id="i_CELLULAR_RETURN_INDEX_0"></span>**CELLULAR_RETURN_INDEX_0** = **0**
- <span id="i_CELLULAR_RETURN_INDEX_0_ADD_1"></span>**CELLULAR_RETURN_INDEX_0_ADD_1** = **1**
- <span id="i_CELLULAR_RETURN_INDEX_0_SUB_1"></span>**CELLULAR_RETURN_INDEX_0_SUB_1** = **2**
- <span id="i_CELLULAR_RETURN_INDEX_0_MUL_1"></span>**CELLULAR_RETURN_INDEX_0_MUL_1** = **3**
- <span id="i_CELLULAR_RETURN_INDEX_0_DIV_1"></span>**CELLULAR_RETURN_INDEX_0_DIV_1** = **4**

enum **SIMDLevel**: 

- <span id="i_SIMD_NULL"></span>**SIMD_NULL** = **0** --- Undefined level.
- <span id="i_SIMD_SCALAR"></span>**SIMD_SCALAR** = **1** --- Single numbers. Slowest level.
- <span id="i_SIMD_SSE"></span>**SIMD_SSE** = **2** --- x86 CPU processing 4 floats in one instruction.
- <span id="i_SIMD_SSE2"></span>**SIMD_SSE2** = **4** --- x86 CPU Processing 4 floats in one instruction, with more instructions supported than SSE.
- <span id="i_SIMD_SSE3"></span>**SIMD_SSE3** = **8** --- x86 CPU Processing 4 floats in one instruction, with more instructions supported than SSE3.
- <span id="i_SIMD_SSSE3"></span>**SIMD_SSSE3** = **16** --- x86 CPU Processing 4 floats in one instruction, with more instructions supported than SSE4.
- <span id="i_SIMD_SSE41"></span>**SIMD_SSE41** = **32** --- x86 CPU Processing 4 floats in one instruction, with more instructions supported than SSE41.
- <span id="i_SIMD_SSE42"></span>**SIMD_SSE42** = **64** --- x86 CPU Processing 4 floats in one instruction, with more instructions supported than SSE42.
- <span id="i_SIMD_AVX"></span>**SIMD_AVX** = **128** --- x86 CPU Processing 8 floats in one instruction.
- <span id="i_SIMD_AVX2"></span>**SIMD_AVX2** = **256** --- x86 CPU Processing 8 floats in one instruction, with more instructions supported than AVX.
- <span id="i_SIMD_AVX512"></span>**SIMD_AVX512** = **512** --- x86 CPU Processing 16 floats in one instruction.
- <span id="i_SIMD_NEON"></span>**SIMD_NEON** = **65536** --- ARM CPU processing 4 floats in one instruction.


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

Fills a greyscale image with noise values.

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_get_noise_2d_single"></span> **get_noise_2d_single**( [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) pos ) 

Generates a single value of 2D noise.

Note that generating values one by one will not benefit from SIMD performance as much as generating multiple values at once.

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_get_noise_3d_single"></span> **get_noise_3d_single**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos ) 

Generates a single value of 3D noise.

Note that generating values one by one will not benefit from SIMD performance as much as generating multiple values at once.

- [String](https://docs.godotengine.org/en/stable/classes/class_string.html)<span id="i_get_simd_level_name"></span> **get_simd_level_name**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) level ) 

Gets which SIMD level was detected by the library. This gives an indication of performance on the current CPU, as different models can have different SIMD instructions available.

- [void](#)<span id="i_update_generator"></span> **update_generator**( ) 

This method must be called after you change properties, so they can take effect.

_Generated on Feb 24, 2024_
