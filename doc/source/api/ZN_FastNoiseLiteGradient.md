# ZN_FastNoiseLiteGradient

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)

Generates coherent and fractal noise gradients using the [FastNoiseLite](https://github.com/Auburn/FastNoiseLite) library.

## Description: 

This is an alternate implementation of [FastNoiseLite](https://docs.godotengine.org/en/stable/classes/class_fastnoiselite.html), because Godot's integration does not expose methods to access gradients directly.

## Properties: 


Type                                                                      | Name                                         | Default 
------------------------------------------------------------------------- | -------------------------------------------- | --------
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [amplitude](#i_amplitude)                    | 30.0    
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [fractal_gain](#i_fractal_gain)              | 0.5     
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [fractal_lacunarity](#i_fractal_lacunarity)  | 2.0     
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)      | [fractal_octaves](#i_fractal_octaves)        | 3       
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)      | [fractal_type](#i_fractal_type)              | 0       
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)      | [noise_type](#i_noise_type)                  | 2       
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [period](#i_period)                          | 64.0    
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)      | [rotation_type_3d](#i_rotation_type_3d)      | 0       
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)      | [seed](#i_seed)                              | 0       
<p></p>

## Methods: 


Return                                                                        | Signature                                                                                                        
----------------------------------------------------------------------------- | -----------------------------------------------------------------------------------------------------------------
[Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html)  | [warp_2d](#i_warp_2d) ( [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) position )  
[Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)  | [warp_3d](#i_warp_3d) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) position )  
<p></p>

## Enumerations: 

enum **NoiseType**: 

- <span id="i_TYPE_OPEN_SIMPLEX_2"></span>**TYPE_OPEN_SIMPLEX_2** = **0**
- <span id="i_TYPE_OPEN_SIMPLEX_2_REDUCED"></span>**TYPE_OPEN_SIMPLEX_2_REDUCED** = **1**
- <span id="i_TYPE_VALUE"></span>**TYPE_VALUE** = **2**

enum **FractalType**: 

- <span id="i_FRACTAL_NONE"></span>**FRACTAL_NONE** = **0**
- <span id="i_FRACTAL_DOMAIN_WARP_PROGRESSIVE"></span>**FRACTAL_DOMAIN_WARP_PROGRESSIVE** = **1**
- <span id="i_FRACTAL_DOMAIN_WARP_INDEPENDENT"></span>**FRACTAL_DOMAIN_WARP_INDEPENDENT** = **2**

enum **RotationType3D**: 

- <span id="i_ROTATION_3D_NONE"></span>**ROTATION_3D_NONE** = **0**
- <span id="i_ROTATION_3D_IMPROVE_XY_PLANES"></span>**ROTATION_3D_IMPROVE_XY_PLANES** = **1**
- <span id="i_ROTATION_3D_IMPROVE_XZ_PLANES"></span>**ROTATION_3D_IMPROVE_XZ_PLANES** = **2**


## Property Descriptions

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_amplitude"></span> **amplitude** = 30.0

*(This property has no documentation)*

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_fractal_gain"></span> **fractal_gain** = 0.5

*(This property has no documentation)*

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_fractal_lacunarity"></span> **fractal_lacunarity** = 2.0

*(This property has no documentation)*

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_fractal_octaves"></span> **fractal_octaves** = 3

*(This property has no documentation)*

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_fractal_type"></span> **fractal_type** = 0

*(This property has no documentation)*

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_noise_type"></span> **noise_type** = 2

*(This property has no documentation)*

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_period"></span> **period** = 64.0

*(This property has no documentation)*

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_rotation_type_3d"></span> **rotation_type_3d** = 0

*(This property has no documentation)*

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_seed"></span> **seed** = 0

*(This property has no documentation)*

## Method Descriptions

### [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html)<span id="i_warp_2d"></span> **warp_2d**( [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) position ) 

*(This method has no documentation)*

### [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)<span id="i_warp_3d"></span> **warp_3d**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) position ) 

*(This method has no documentation)*

_Generated on Apr 06, 2024_
