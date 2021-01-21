# VoxelTool

Inherits: [Reference](https://docs.godotengine.org/en/stable/classes/class_reference.html)


Helper class to easily access and modify voxels

## Properties: 


Type   | Name                             | Default 
------ | -------------------------------- | --------
`int`  | [channel](#i_channel)            | 0       
`int`  | [eraser_value](#i_eraser_value)  | 0       
`int`  | [mode](#i_mode)                  | 0       
`int`  | [value](#i_value)                | 0       
<p></p>

## Methods: 


Return                                                                        | Signature                                                                                                                                                                                                                                                                                                                                                                                          
----------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                                     | [do_box](#i_do_box) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) begin, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) end )                                                                                                                                                                                                       
[void](#)                                                                     | [do_point](#i_do_point) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos )                                                                                                                                                                                                                                                                                       
[void](#)                                                                     | [do_sphere](#i_do_sphere) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) center, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) radius )                                                                                                                                                                                                 
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)          | [get_voxel](#i_get_voxel) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos )                                                                                                                                                                                                                                                                                     
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)      | [get_voxel_f](#i_get_voxel_f) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos )                                                                                                                                                                                                                                                                                 
[Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)  | [get_voxel_metadata](#i_get_voxel_metadata) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos )                                                                                                                                                                                                                                                                   
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)        | [is_area_editable](#i_is_area_editable) ( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) box )                                                                                                                                                                                                                                                                             
[void](#)                                                                     | [paste](#i_paste) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) dst_pos, [Reference](https://docs.godotengine.org/en/stable/classes/class_reference.html) src_buffer, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_mask_value )                                                                                                       
[VoxelRaycastResult](VoxelRaycastResult.md)                                   | [raycast](#i_raycast) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) direction, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) max_distance=10.0, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) collision_mask=4294967295 )  
[void](#)                                                                     | [set_voxel](#i_set_voxel) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) v )                                                                                                                                                                                                             
[void](#)                                                                     | [set_voxel_f](#i_set_voxel_f) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) v )                                                                                                                                                                                                     
[void](#)                                                                     | [set_voxel_metadata](#i_set_voxel_metadata) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) meta )                                                                                                                                                                                
<p></p>

## Enumerations: 

enum **Mode**: 

- **MODE_ADD** = **0**
- **MODE_REMOVE** = **1**
- **MODE_SET** = **2**


## Property Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_channel"></span> **channel** = 0


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_eraser_value"></span> **eraser_value** = 0


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_mode"></span> **mode** = 0


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_value"></span> **value** = 0


## Method Descriptions

- [void](#)<span id="i_do_box"></span> **do_box**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) begin, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) end ) 

Operate on a rectangular cuboid section of the terrain. `begin` and `end` are inclusive. Choose operation and which voxel to use by setting `value` and `mode` before calling this function.

- [void](#)<span id="i_do_point"></span> **do_point**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos ) 



- [void](#)<span id="i_do_sphere"></span> **do_sphere**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) center, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) radius ) 



- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_voxel"></span> **get_voxel**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos ) 



- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_get_voxel_f"></span> **get_voxel_f**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos ) 



- [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)<span id="i_get_voxel_metadata"></span> **get_voxel_metadata**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos ) 



- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_is_area_editable"></span> **is_area_editable**( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) box ) 



- [void](#)<span id="i_paste"></span> **paste**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) dst_pos, [Reference](https://docs.godotengine.org/en/stable/classes/class_reference.html) src_buffer, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_mask_value ) 



- [VoxelRaycastResult](VoxelRaycastResult.md)<span id="i_raycast"></span> **raycast**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) direction, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) max_distance=10.0, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) collision_mask=4294967295 ) 



- [void](#)<span id="i_set_voxel"></span> **set_voxel**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) v ) 



- [void](#)<span id="i_set_voxel_f"></span> **set_voxel_f**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) v ) 



- [void](#)<span id="i_set_voxel_metadata"></span> **set_voxel_metadata**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) meta ) 



_Generated on Jan 20, 2021_
