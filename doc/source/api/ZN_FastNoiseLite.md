# ZN_FastNoiseLite

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)


Generates coherent and fractal noise using the [FastNoiseLite](https://github.com/Auburn/FastNoise) library.

## Properties: 


Type                        | Name                                                         | Default 
--------------------------- | ------------------------------------------------------------ | --------
`int`                       | [cellular_distance_function](#i_cellular_distance_function)  | 1       
`float`                     | [cellular_jitter](#i_cellular_jitter)                        | 1.0     
`int`                       | [cellular_return_type](#i_cellular_return_type)              | 1       
`float`                     | [fractal_gain](#i_fractal_gain)                              | 0.5     
`float`                     | [fractal_lacunarity](#i_fractal_lacunarity)                  | 2.0     
`int`                       | [fractal_octaves](#i_fractal_octaves)                        | 3       
`float`                     | [fractal_ping_pong_strength](#i_fractal_ping_pong_strength)  | 2.0     
`int`                       | [fractal_type](#i_fractal_type)                              | 1       
`float`                     | [fractal_weighted_strength](#i_fractal_weighted_strength)    | 0.0     
`int`                       | [noise_type](#i_noise_type)                                  | 0       
`float`                     | [period](#i_period)                                          | 64.0    
`int`                       | [rotation_type_3d](#i_rotation_type_3d)                      | 0       
`int`                       | [seed](#i_seed)                                              | 0       
`ZN_FastNoiseLiteGradient`  | [warp_noise](#i_warp_noise)                                  |         
<p></p>

## Methods: 


Return                                                                    | Signature                                                                                                                                                                                                                                                               
------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [get_noise_2d](#i_get_noise_2d) ( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) x, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) y )                                                                              
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [get_noise_2dv](#i_get_noise_2dv) ( [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) position )                                                                                                                                             
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [get_noise_3d](#i_get_noise_3d) ( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) x, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) y, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) z )  
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [get_noise_3dv](#i_get_noise_3dv) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) position )                                                                                                                                             
<p></p>

## Enumerations: 

enum **NoiseType**: 

- **TYPE_OPEN_SIMPLEX_2** = **0**
- **TYPE_OPEN_SIMPLEX_2S** = **1**
- **TYPE_CELLULAR** = **2**
- **TYPE_PERLIN** = **3**
- **TYPE_VALUE_CUBIC** = **4**
- **TYPE_VALUE** = **5**

enum **FractalType**: 

- **FRACTAL_NONE** = **0**
- **FRACTAL_FBM** = **1**
- **FRACTAL_RIDGED** = **2**
- **FRACTAL_PING_PONG** = **3**

enum **RotationType3D**: 

- **ROTATION_3D_NONE** = **0**
- **ROTATION_3D_IMPROVE_XY_PLANES** = **1**
- **ROTATION_3D_IMPROVE_XZ_PLANES** = **2**

enum **CellularDistanceFunction**: 

- **CELLULAR_DISTANCE_EUCLIDEAN** = **0**
- **CELLULAR_DISTANCE_EUCLIDEAN_SQ** = **1**
- **CELLULAR_DISTANCE_MANHATTAN** = **2**
- **CELLULAR_DISTANCE_HYBRID** = **3**

enum **CellularReturnType**: 

- **CELLULAR_RETURN_CELL_VALUE** = **0**
- **CELLULAR_RETURN_DISTANCE** = **1**
- **CELLULAR_RETURN_DISTANCE_2** = **2**
- **CELLULAR_RETURN_DISTANCE_2_ADD** = **3**
- **CELLULAR_RETURN_DISTANCE_2_SUB** = **4**
- **CELLULAR_RETURN_DISTANCE_2_MUL** = **5**
- **CELLULAR_RETURN_DISTANCE_2_DIV** = **6**


## Property Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_cellular_distance_function"></span> **cellular_distance_function** = 1


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_cellular_jitter"></span> **cellular_jitter** = 1.0


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_cellular_return_type"></span> **cellular_return_type** = 1


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_fractal_gain"></span> **fractal_gain** = 0.5


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_fractal_lacunarity"></span> **fractal_lacunarity** = 2.0


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_fractal_octaves"></span> **fractal_octaves** = 3


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_fractal_ping_pong_strength"></span> **fractal_ping_pong_strength** = 2.0


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_fractal_type"></span> **fractal_type** = 1


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_fractal_weighted_strength"></span> **fractal_weighted_strength** = 0.0


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_noise_type"></span> **noise_type** = 0


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_period"></span> **period** = 64.0


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_rotation_type_3d"></span> **rotation_type_3d** = 0


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_seed"></span> **seed** = 0


- [ZN_FastNoiseLiteGradient](ZN_FastNoiseLiteGradient.md)<span id="i_warp_noise"></span> **warp_noise**


## Method Descriptions

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_get_noise_2d"></span> **get_noise_2d**( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) x, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) y ) 


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_get_noise_2dv"></span> **get_noise_2dv**( [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) position ) 


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_get_noise_3d"></span> **get_noise_3d**( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) x, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) y, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) z ) 


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_get_noise_3dv"></span> **get_noise_3dv**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) position ) 


_Generated on Mar 26, 2023_
