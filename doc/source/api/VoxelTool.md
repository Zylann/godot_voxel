# VoxelTool

Inherits: [RefCounted](https://docs.godotengine.org/en/stable/classes/class_refcounted.html)


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
`float`  | [sdf_strength](#i_sdf_strength)        |         
`float`  | [texture_falloff](#i_texture_falloff)  |         
`int`    | [texture_index](#i_texture_index)      |         
`float`  | [texture_opacity](#i_texture_opacity)  |         
`int`    | [value](#i_value)                      |         
<p></p>

## Methods: 


Return                                                                          | Signature                                                                                                                                                                                                                                                                                                                                                                                          
------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)            | [color_to_u16](#i_color_to_u16) ( [Color](https://docs.godotengine.org/en/stable/classes/class_color.html) color ) static                                                                                                                                                                                                                                                                          
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)            | [color_to_u16_weights](#i_color_to_u16_weights) ( [Color](https://docs.godotengine.org/en/stable/classes/class_color.html) _unnamed_arg0 ) static                                                                                                                                                                                                                                                  
[void](#)                                                                       | [copy](#i_copy) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_pos, [VoxelBuffer](VoxelBuffer.md) dst_buffer, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channels_mask )                                                                                                                                                           
[void](#)                                                                       | [do_box](#i_do_box) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) begin, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) end )                                                                                                                                                                                                   
[void](#)                                                                       | [do_point](#i_do_point) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos )                                                                                                                                                                                                                                                                                     
[void](#)                                                                       | [do_sphere](#i_do_sphere) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) center, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) radius )                                                                                                                                                                                                 
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)            | [get_voxel](#i_get_voxel) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos )                                                                                                                                                                                                                                                                                   
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)        | [get_voxel_f](#i_get_voxel_f) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos )                                                                                                                                                                                                                                                                               
[Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)    | [get_voxel_metadata](#i_get_voxel_metadata) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos ) const                                                                                                                                                                                                                                                           
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)          | [is_area_editable](#i_is_area_editable) ( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) box ) const                                                                                                                                                                                                                                                                       
[Color](https://docs.godotengine.org/en/stable/classes/class_color.html)        | [normalize_color](#i_normalize_color) ( [Color](https://docs.godotengine.org/en/stable/classes/class_color.html) _unnamed_arg0 ) static                                                                                                                                                                                                                                                            
[void](#)                                                                       | [paste](#i_paste) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) dst_pos, [VoxelBuffer](VoxelBuffer.md) src_buffer, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channels_mask, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_mask_value )                                                                    
[VoxelRaycastResult](VoxelRaycastResult.md)                                     | [raycast](#i_raycast) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) direction, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) max_distance=10.0, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) collision_mask=4294967295 )  
[void](#)                                                                       | [set_voxel](#i_set_voxel) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) v )                                                                                                                                                                                                           
[void](#)                                                                       | [set_voxel_f](#i_set_voxel_f) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) v )                                                                                                                                                                                                   
[void](#)                                                                       | [set_voxel_metadata](#i_set_voxel_metadata) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) meta )                                                                                                                                                                              
[Vector4i](https://docs.godotengine.org/en/stable/classes/class_vector4i.html)  | [u16_indices_to_vec4i](#i_u16_indices_to_vec4i) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) _unnamed_arg0 ) static                                                                                                                                                                                                                                                      
[Color](https://docs.godotengine.org/en/stable/classes/class_color.html)        | [u16_weights_to_color](#i_u16_weights_to_color) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) _unnamed_arg0 ) static                                                                                                                                                                                                                                                      
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)            | [vec4i_to_u16_indices](#i_vec4i_to_u16_indices) ( [Vector4i](https://docs.godotengine.org/en/stable/classes/class_vector4i.html) _unnamed_arg0 ) static                                                                                                                                                                                                                                            
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

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_sdf_strength"></span> **sdf_strength**


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_texture_falloff"></span> **texture_falloff**


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_texture_index"></span> **texture_index**


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_texture_opacity"></span> **texture_opacity**


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_value"></span> **value**

Sets which voxel value will be used. This is not relevant when editing enum VoxelBuffer.CHANNEL_SDF.

## Method Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_color_to_u16"></span> **color_to_u16**( [Color](https://docs.godotengine.org/en/stable/classes/class_color.html) color ) 

Encodes normalized 4-float color into 16-bit integer data. It is used with the COLOR channel, in cases where the channel represents direct colors (without using a palette).

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_color_to_u16_weights"></span> **color_to_u16_weights**( [Color](https://docs.godotengine.org/en/stable/classes/class_color.html) _unnamed_arg0 ) 

Encodes normalized 4-float color into 16-bit integer data, for use with the WEIGHTS channel.

- [void](#)<span id="i_copy"></span> **copy**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_pos, [VoxelBuffer](VoxelBuffer.md) dst_buffer, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channels_mask ) 

Copies voxels in a box and stores them in the passed buffer.

`src_pos` is the lowest corner of the box, and its size is determined by the size of `dst_buffer`.

`channels_mask` is a bitmask where each bit tells which channels will be copied. Example: `1 << VoxelBuffer.CHANNEL_SDF` to get only SDF data. Use `0xff` if you want them all.

- [void](#)<span id="i_do_box"></span> **do_box**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) begin, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) end ) 

Operate on a rectangular cuboid section of the terrain. `begin` and `end` are inclusive. Choose operation and which voxel to use by setting `value` and `mode` before calling this function.

- [void](#)<span id="i_do_point"></span> **do_point**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos ) 


- [void](#)<span id="i_do_sphere"></span> **do_sphere**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) center, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) radius ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_voxel"></span> **get_voxel**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos ) 


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_get_voxel_f"></span> **get_voxel_f**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos ) 


- [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)<span id="i_get_voxel_metadata"></span> **get_voxel_metadata**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos ) 


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_is_area_editable"></span> **is_area_editable**( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) box ) 


- [Color](https://docs.godotengine.org/en/stable/classes/class_color.html)<span id="i_normalize_color"></span> **normalize_color**( [Color](https://docs.godotengine.org/en/stable/classes/class_color.html) _unnamed_arg0 ) 


- [void](#)<span id="i_paste"></span> **paste**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) dst_pos, [VoxelBuffer](VoxelBuffer.md) src_buffer, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channels_mask, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) src_mask_value ) 

Paste voxels in a box from the given buffer at a specific location.

`dst_pos` is the lowest corner of the box, and its size is determined by the size of `src_buffer`.

`channels_mask` is a bitmask where each bit tells which channels will be modified. Example: `1 << VoxelBuffer.CHANNEL_SDF` only write SDF data. Use `0xff` if you want them all.

`src_mask_value` For every pasted channel, if voxel data has this value, it will not be pasted. This is mostly useful when using blocky voxel types. Pass `0xffffffff` to disable masking.

- [VoxelRaycastResult](VoxelRaycastResult.md)<span id="i_raycast"></span> **raycast**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) origin, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) direction, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) max_distance=10.0, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) collision_mask=4294967295 ) 

Runs a voxel-based raycast to find the first hit from an origin and a direction.

This is useful when colliders cannot be relied upon. It might also be faster (at least at short range), and is more precise to find which voxel is hit. It internally uses the DDA algorithm.

`collision_mask` is currently only used with blocky voxels. It is combined with [VoxelBlockyModel.collision_mask](https://docs.godotengine.org/en/stable/classes/class_voxelblockymodel.collision_mask.html) to decide which voxel types the ray can collide with.

- [void](#)<span id="i_set_voxel"></span> **set_voxel**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) v ) 

Sets the raw integer value of a specific voxel on the current channel.

- [void](#)<span id="i_set_voxel_f"></span> **set_voxel_f**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) v ) 

Sets the normalized decimal value of a specific voxel. This should preferably be used when modifying voxels on the SDF channel.

- [void](#)<span id="i_set_voxel_metadata"></span> **set_voxel_metadata**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) meta ) 


- [Vector4i](https://docs.godotengine.org/en/stable/classes/class_vector4i.html)<span id="i_u16_indices_to_vec4i"></span> **u16_indices_to_vec4i**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) _unnamed_arg0 ) 

Decodes raw voxel integer data from the INDICES channel into a 4-integer vector.

- [Color](https://docs.godotengine.org/en/stable/classes/class_color.html)<span id="i_u16_weights_to_color"></span> **u16_weights_to_color**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) _unnamed_arg0 ) 

Decodes raw voxel integer data from the WEIGHTS channel into a normalized 4-float color.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_vec4i_to_u16_indices"></span> **vec4i_to_u16_indices**( [Vector4i](https://docs.godotengine.org/en/stable/classes/class_vector4i.html) _unnamed_arg0 ) 

Encodes a 4-integer vector into 16-bit integer voxel data, for use in the INDICES channel.

_Generated on Mar 26, 2023_
