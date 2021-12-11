# VoxelTool

Inherits: [Reference](https://docs.godotengine.org/en/stable/classes/class_reference.html)


Helper class to easily access and modify voxels

## Description: 

Abstract interface to access and edit voxels. It allows accessing individual voxels, or doing bulk operations such as carving large chunks or copy/paste boxes.

It's not a class to instantiate alone, you may get it from the voxel objects you want to work with.

## Properties: 


Type     | Name                                   | Default 
-------- | -------------------------------------- | --------
`int`    | [channel](#i_channel)                  |         
`int`    | [eraser_value](#i_eraser_value)        |         
`int`    | [mode](#i_mode)                        |         
`float`  | [sdf_scale](#i_sdf_scale)              |         
`float`  | [texture_falloff](#i_texture_falloff)  |         
`int`    | [texture_index](#i_texture_index)      |         
`float`  | [texture_opacity](#i_texture_opacity)  |         
`int`    | [value](#i_value)                      |         
<p></p>

## Methods: 


Return                                                                        | Signature                                                                                                                                                                                                                                                                                                                                                                                          
----------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                                     | [copy](#i_copy) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) src_pos, [Reference](https://docs.godotengine.org/en/stable/classes/class_reference.html) dst_buffer, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channels_mask )                                                                                                          
[void](#)                                                                     | [do_box](#i_do_box) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) begin, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) end )                                                                                                                                                                                                       
[void](#)                                                                     | [do_point](#i_do_point) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos )                                                                                                                                                                                                                                                                                       
[void](#)                                                                     | [do_sphere](#i_do_sphere) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) center, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) radius )                                                                                                                                                                                                 
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)          | [get_voxel](#i_get_voxel) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos )                                                                                                                                                                                                                                                                                     
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)      | [get_voxel_f](#i_get_voxel_f) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos )                                                                                                                                                                                                                                                                                 
[Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)  | [get_voxel_metadata](#i_get_voxel_metadata) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos ) const                                                                                                                                                                                                                                                             
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)        | [is_area_editable](#i_is_area_editable) ( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) box ) const                                                                                                                                                                                                                                                                       
[void](#)                                                                     | [paste](#i_paste) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) dst_pos, [Reference](https://docs.godotengine.org/en/stable/classes/class_reference.html) src_buffer, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channels_mask, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_mask_value )                   
[VoxelRaycastResult](VoxelRaycastResult.md)                                   | [raycast](#i_raycast) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) direction, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) max_distance=10.0, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) collision_mask=4294967295 )  
[void](#)                                                                     | [set_voxel](#i_set_voxel) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) v )                                                                                                                                                                                                             
[void](#)                                                                     | [set_voxel_f](#i_set_voxel_f) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) v )                                                                                                                                                                                                     
[void](#)                                                                     | [set_voxel_metadata](#i_set_voxel_metadata) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) meta )                                                                                                                                                                                
<p></p>

## Enumerations: 

enum **Mode**: 

- **MODE_ADD** = **0** --- When editing [enum VoxelBuffer.CHANNEL_SDF], will add matter. Useful for building.
- **MODE_REMOVE** = **1** --- When editing [enum VoxelBuffer.CHANNEL_SDF], will subtract matter. Useful for digging.
- **MODE_SET** = **2** --- Replace voxel values without any blending. Useful for blocky voxels.
- **MODE_TEXTURE_PAINT** = **3**


## Property Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_channel"></span> **channel**

Set which channel will be edited. When used on a terrain node, it will default to the first available channel, based on the stream and generator.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_eraser_value"></span> **eraser_value**

Sets which value will be used to erase voxels when editing the enum VoxelBuffer.CHANNEL_TYPE channel in enum MODE_REMOVE mode.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_mode"></span> **mode**

Sets how `do_*` functions will behave. This may vary depending on the channel.

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_sdf_scale"></span> **sdf_scale**

When working with smooth voxels, applies a scale to the signed distance field. A high scale (1 or higher) will tend to produce blocky results, and a low scale (below 1, but not too close to zero) will tend to be smoother.



This is related to the enum VoxelBuffer.Depth configuration on voxels. For 8-bit and 16-bit, there is a limited range of values the Signed Distance Field can take, and by default it is clamped to -1..1, so the gradient can only range across 2 voxels. But when LOD is used, it is better to stretch that range over a longer distance, and this is achieved by scaling SDF values.

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_texture_falloff"></span> **texture_falloff**


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_texture_index"></span> **texture_index**


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_texture_opacity"></span> **texture_opacity**


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_value"></span> **value**

Sets which voxel value will be used. This is not relevant when editing enum VoxelBuffer.CHANNEL_SDF.

## Method Descriptions

- [void](#)<span id="i_copy"></span> **copy**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) src_pos, [Reference](https://docs.godotengine.org/en/stable/classes/class_reference.html) dst_buffer, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channels_mask ) 


- [void](#)<span id="i_do_box"></span> **do_box**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) begin, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) end ) 

Operate on a rectangular cuboid section of the terrain. `begin` and `end` are inclusive. Choose operation and which voxel to use by setting `value` and `mode` before calling this function.

- [void](#)<span id="i_do_point"></span> **do_point**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos ) 


- [void](#)<span id="i_do_sphere"></span> **do_sphere**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) center, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) radius ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_voxel"></span> **get_voxel**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos ) 


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_get_voxel_f"></span> **get_voxel_f**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos ) 


- [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)<span id="i_get_voxel_metadata"></span> **get_voxel_metadata**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos ) 


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_is_area_editable"></span> **is_area_editable**( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) box ) 


- [void](#)<span id="i_paste"></span> **paste**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) dst_pos, [Reference](https://docs.godotengine.org/en/stable/classes/class_reference.html) src_buffer, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channels_mask, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_mask_value ) 


- [VoxelRaycastResult](VoxelRaycastResult.md)<span id="i_raycast"></span> **raycast**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) direction, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) max_distance=10.0, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) collision_mask=4294967295 ) 


- [void](#)<span id="i_set_voxel"></span> **set_voxel**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) v ) 


- [void](#)<span id="i_set_voxel_f"></span> **set_voxel_f**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) v ) 


- [void](#)<span id="i_set_voxel_metadata"></span> **set_voxel_metadata**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) meta ) 


_Generated on Nov 06, 2021_
