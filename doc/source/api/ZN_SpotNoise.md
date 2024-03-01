# ZN_SpotNoise

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)

Specialization of cellular noise intented for cheap "ore patch" generation or deterministic point scattering.

## Description: 

Divides space into a grid where each cell contains a circular "spot". Noise evaluation returns 1 when the position is inside a spot, 0 otherwise. 

Limitation: high jitter can make spots clip with cell borders. This is expected. If you need higher quality (but slower) results, you may use another noise library such as [FastNoiseLite](https://docs.godotengine.org/en/stable/classes/class_fastnoiselite.html).

## Properties: 


Type     | Name                           | Default 
-------- | ------------------------------ | --------
`float`  | [cell_size](#i_cell_size)      | 32.0    
`float`  | [jitter](#i_jitter)            | 0.9     
`int`    | [seed](#i_seed)                | 1337    
`float`  | [spot_radius](#i_spot_radius)  | 3.0     
<p></p>

## Methods: 


Return                                                                                              | Signature                                                                                                                                                                                                                                                                    
--------------------------------------------------------------------------------------------------- | -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                            | [get_noise_2d](#i_get_noise_2d) ( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) x, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) y ) const                                                                             
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                            | [get_noise_2dv](#i_get_noise_2dv) ( [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) pos ) const                                                                                                                                                 
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                            | [get_noise_3d](#i_get_noise_3d) ( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) x, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) y, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) z ) const 
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                            | [get_noise_3dv](#i_get_noise_3dv) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos ) const                                                                                                                                                 
[PackedVector2Array](https://docs.godotengine.org/en/stable/classes/class_packedvector2array.html)  | [get_spot_positions_in_area_2d](#i_get_spot_positions_in_area_2d) ( [Rect2](https://docs.godotengine.org/en/stable/classes/class_rect2.html) rect ) const                                                                                                                    
[PackedVector3Array](https://docs.godotengine.org/en/stable/classes/class_packedvector3array.html)  | [get_spot_positions_in_area_3d](#i_get_spot_positions_in_area_3d) ( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) aabb ) const                                                                                                                      
<p></p>

## Property Descriptions

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_cell_size"></span> **cell_size** = 32.0


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_jitter"></span> **jitter** = 0.9


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_seed"></span> **seed** = 1337


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_spot_radius"></span> **spot_radius** = 3.0


## Method Descriptions

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_get_noise_2d"></span> **get_noise_2d**( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) x, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) y ) 


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_get_noise_2dv"></span> **get_noise_2dv**( [Vector2](https://docs.godotengine.org/en/stable/classes/class_vector2.html) pos ) 


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_get_noise_3d"></span> **get_noise_3d**( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) x, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) y, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) z ) 


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_get_noise_3dv"></span> **get_noise_3dv**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos ) 


- [PackedVector2Array](https://docs.godotengine.org/en/stable/classes/class_packedvector2array.html)<span id="i_get_spot_positions_in_area_2d"></span> **get_spot_positions_in_area_2d**( [Rect2](https://docs.godotengine.org/en/stable/classes/class_rect2.html) rect ) 


- [PackedVector3Array](https://docs.godotengine.org/en/stable/classes/class_packedvector3array.html)<span id="i_get_spot_positions_in_area_3d"></span> **get_spot_positions_in_area_3d**( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) aabb ) 


_Generated on Feb 24, 2024_
