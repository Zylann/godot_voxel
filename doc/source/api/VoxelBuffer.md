# VoxelBuffer

Inherits: [RefCounted](https://docs.godotengine.org/en/stable/classes/class_refcounted.html)

3D grid storing voxel data.

## Description: 

This contains dense voxels data storage (every single cell holds data, there is no sparse optimization of space). Works like a normal 3D grid containing a voxel value in each cell. Organized in channels of configurable bit depth. Values can be interpreted either as unsigned integers, fixed-point or floats. See [Depth](VoxelBuffer.md#enumerations) for more information.

Arbitrary metadata can also be stored, either for the whole buffer, or per-voxel, at higher cost. This metadata can get saved and loaded along voxels, however you must make sure the data is serializable (i.e it should not contain nodes or arbitrary objects).

## Methods: 


Return                                                                                        | Signature                                                                                                                                                                                                                                                                                                                                                                                                                                                                                
--------------------------------------------------------------------------------------------- | -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                                                     | [clear](#i_clear) ( )                                                                                                                                                                                                                                                                                                                                                                                                                                                                    
[void](#)                                                                                     | [clear_voxel_metadata](#i_clear_voxel_metadata) ( )                                                                                                                                                                                                                                                                                                                                                                                                                                      
[void](#)                                                                                     | [clear_voxel_metadata_in_area](#i_clear_voxel_metadata_in_area) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) min_pos, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) max_pos )                                                                                                                                                                                                                                       
[void](#)                                                                                     | [compress_uniform_channels](#i_compress_uniform_channels) ( )                                                                                                                                                                                                                                                                                                                                                                                                                            
[void](#)                                                                                     | [copy_channel_from](#i_copy_channel_from) ( [VoxelBuffer](VoxelBuffer.md) other, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel )                                                                                                                                                                                                                                                                                                                          
[void](#)                                                                                     | [copy_channel_from_area](#i_copy_channel_from_area) ( [VoxelBuffer](VoxelBuffer.md) other, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_min, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_max, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) dst_min, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel )                                        
[void](#)                                                                                     | [copy_voxel_metadata_in_area](#i_copy_voxel_metadata_in_area) ( [VoxelBuffer](VoxelBuffer.md) src_buffer, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_min_pos, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_max_pos, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) dst_min_pos )                                                                                           
[void](#)                                                                                     | [create](#i_create) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) sx, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) sy, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) sz )                                                                                                                                                                                                                                        
[ImageTexture3D](https://docs.godotengine.org/en/stable/classes/class_imagetexture3d.html)    | [create_3d_texture_from_sdf_zxy](#i_create_3d_texture_from_sdf_zxy) ( [Format](https://docs.godotengine.org/en/stable/classes/class_image.html#enum-image-format) output_format ) const                                                                                                                                                                                                                                                                                                  
[Image[]](https://docs.godotengine.org/en/stable/classes/class_image[].html)                  | [debug_print_sdf_y_slices](#i_debug_print_sdf_y_slices) ( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) scale=1.0 ) const                                                                                                                                                                                                                                                                                                                                     
[void](#)                                                                                     | [decompress_channel](#i_decompress_channel) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel )                                                                                                                                                                                                                                                                                                                                                             
[void](#)                                                                                     | [downscale_to](#i_downscale_to) ( [VoxelBuffer](VoxelBuffer.md) dst, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_min, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_max, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) dst_min ) const                                                                                                                                      
[void](#)                                                                                     | [fill](#i_fill) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 )                                                                                                                                                                                                                                                                                                           
[void](#)                                                                                     | [fill_area](#i_fill_area) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) min, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) max, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 )                                                                                                                         
[void](#)                                                                                     | [fill_area_f](#i_fill_area_f) ( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) value, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) min, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) max, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel )                                                                                                                   
[void](#)                                                                                     | [fill_f](#i_fill_f) ( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 )                                                                                                                                                                                                                                                                                                   
[void](#)                                                                                     | [for_each_voxel_metadata](#i_for_each_voxel_metadata) ( [Callable](https://docs.godotengine.org/en/stable/classes/class_callable.html) callback ) const                                                                                                                                                                                                                                                                                                                                  
[void](#)                                                                                     | [for_each_voxel_metadata_in_area](#i_for_each_voxel_metadata_in_area) ( [Callable](https://docs.godotengine.org/en/stable/classes/class_callable.html) callback, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) min_pos, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) max_pos )                                                                                                                                        
[Allocator](VoxelBuffer.md#enumerations)                                                      | [get_allocator](#i_get_allocator) ( ) const                                                                                                                                                                                                                                                                                                                                                                                                                                              
[Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)                  | [get_block_metadata](#i_get_block_metadata) ( ) const                                                                                                                                                                                                                                                                                                                                                                                                                                    
[PackedByteArray](https://docs.godotengine.org/en/stable/classes/class_packedbytearray.html)  | [get_channel_as_byte_array](#i_get_channel_as_byte_array) ( [ChannelId](VoxelBuffer.md#enumerations) channel_index ) const                                                                                                                                                                                                                                                                                                                                                               
[Compression](VoxelBuffer.md#enumerations)                                                    | [get_channel_compression](#i_get_channel_compression) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) const                                                                                                                                                                                                                                                                                                                                             
[Depth](VoxelBuffer.md#enumerations)                                                          | [get_channel_depth](#i_get_channel_depth) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) const                                                                                                                                                                                                                                                                                                                                                         
[Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)                | [get_size](#i_get_size) ( ) const                                                                                                                                                                                                                                                                                                                                                                                                                                                        
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                          | [get_voxel](#i_get_voxel) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) const                                                                                                                                               
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                      | [get_voxel_f](#i_get_voxel_f) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) const                                                                                                                                           
[Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)                  | [get_voxel_metadata](#i_get_voxel_metadata) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos ) const                                                                                                                                                                                                                                                                                                                                                 
[VoxelTool](VoxelTool.md)                                                                     | [get_voxel_tool](#i_get_voxel_tool) ( )                                                                                                                                                                                                                                                                                                                                                                                                                                                  
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)                        | [is_uniform](#i_is_uniform) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) const                                                                                                                                                                                                                                                                                                                                                                       
[void](#)                                                                                     | [mirror](#i_mirror) ( [Axis](https://docs.godotengine.org/en/stable/classes/class_vector3i.html#enum-vector3i-axis) axis )                                                                                                                                                                                                                                                                                                                                                               
[void](#)                                                                                     | [op_add_buffer_f](#i_op_add_buffer_f) ( [VoxelBuffer](VoxelBuffer.md) other, [ChannelId](VoxelBuffer.md#enumerations) channel )                                                                                                                                                                                                                                                                                                                                                          
[void](#)                                                                                     | [op_max_buffer_f](#i_op_max_buffer_f) ( [VoxelBuffer](VoxelBuffer.md) other, [ChannelId](VoxelBuffer.md#enumerations) channel )                                                                                                                                                                                                                                                                                                                                                          
[void](#)                                                                                     | [op_min_buffer_f](#i_op_min_buffer_f) ( [VoxelBuffer](VoxelBuffer.md) other, [ChannelId](VoxelBuffer.md#enumerations) channel )                                                                                                                                                                                                                                                                                                                                                          
[void](#)                                                                                     | [op_mul_buffer_f](#i_op_mul_buffer_f) ( [VoxelBuffer](VoxelBuffer.md) other, [ChannelId](VoxelBuffer.md#enumerations) channel )                                                                                                                                                                                                                                                                                                                                                          
[void](#)                                                                                     | [op_mul_value_f](#i_op_mul_value_f) ( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) other, [ChannelId](VoxelBuffer.md#enumerations) channel )                                                                                                                                                                                                                                                                                                                 
[void](#)                                                                                     | [op_select_less_src_f_dst_i_values](#i_op_select_less_src_f_dst_i_values) ( [VoxelBuffer](VoxelBuffer.md) src, [ChannelId](VoxelBuffer.md#enumerations) src_channel, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) threshold, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value_if_less, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value_if_more, [ChannelId](VoxelBuffer.md#enumerations) dst_channel )  
[void](#)                                                                                     | [op_sub_buffer_f](#i_op_sub_buffer_f) ( [VoxelBuffer](VoxelBuffer.md) other, [ChannelId](VoxelBuffer.md#enumerations) channel )                                                                                                                                                                                                                                                                                                                                                          
[void](#)                                                                                     | [remap_values](#i_remap_values) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel, [PackedInt32Array](https://docs.godotengine.org/en/stable/classes/class_packedint32array.html) map )                                                                                                                                                                                                                                                                     
[void](#)                                                                                     | [rotate_90](#i_rotate_90) ( [Axis](https://docs.godotengine.org/en/stable/classes/class_vector3i.html#enum-vector3i-axis) axis, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) turns )                                                                                                                                                                                                                                                                             
[void](#)                                                                                     | [set_block_metadata](#i_set_block_metadata) ( [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) meta )                                                                                                                                                                                                                                                                                                                                                        
[void](#)                                                                                     | [set_channel_depth](#i_set_channel_depth) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel, [Depth](VoxelBuffer.md#enumerations) depth )                                                                                                                                                                                                                                                                                                                   
[void](#)                                                                                     | [set_channel_from_byte_array](#i_set_channel_from_byte_array) ( [ChannelId](VoxelBuffer.md#enumerations) channel_index, [PackedByteArray](https://docs.godotengine.org/en/stable/classes/class_packedbytearray.html) data )                                                                                                                                                                                                                                                              
[void](#)                                                                                     | [set_voxel](#i_set_voxel) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 )                                                                         
[void](#)                                                                                     | [set_voxel_f](#i_set_voxel_f) ( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 )                                                                 
[void](#)                                                                                     | [set_voxel_metadata](#i_set_voxel_metadata) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) value )                                                                                                                                                                                                                                                                   
[void](#)                                                                                     | [set_voxel_v](#i_set_voxel_v) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 )                                                                                                                                                                                                         
[void](#)                                                                                     | [update_3d_texture_from_sdf_zxy](#i_update_3d_texture_from_sdf_zxy) ( [ImageTexture3D](https://docs.godotengine.org/en/stable/classes/class_imagetexture3d.html) existing_texture ) const                                                                                                                                                                                                                                                                                                
<p></p>

## Enumerations: 

enum **ChannelId**: 

- <span id="i_CHANNEL_TYPE"></span>**CHANNEL_TYPE** = **0** --- Channel used to store voxel types. Used by [VoxelMesherBlocky](VoxelMesherBlocky.md).
- <span id="i_CHANNEL_SDF"></span>**CHANNEL_SDF** = **1** --- Channel used to store SDF data (signed distance field). Used by [VoxelMesherTransvoxel](VoxelMesherTransvoxel.md) and other smooth meshers. Values should preferably be accessed as floats. Negative values are below the isosurface (inside matter), and positive values are above the surface (outside matter).
- <span id="i_CHANNEL_COLOR"></span>**CHANNEL_COLOR** = **2** --- Channel used to store color data. Used by [VoxelMesherCubes](VoxelMesherCubes.md).
- <span id="i_CHANNEL_INDICES"></span>**CHANNEL_INDICES** = **3** --- Channel used to store material indices. Used with smooth voxels.
- <span id="i_CHANNEL_WEIGHTS"></span>**CHANNEL_WEIGHTS** = **4** --- Channel used to store material weights, when more than one index can be stored per voxel. Used with smooth voxels.
- <span id="i_CHANNEL_DATA5"></span>**CHANNEL_DATA5** = **5** --- Free channel. Not used by the engine yet.
- <span id="i_CHANNEL_DATA6"></span>**CHANNEL_DATA6** = **6** --- Free channel. Not used by the engine yet.
- <span id="i_CHANNEL_DATA7"></span>**CHANNEL_DATA7** = **7** --- Free channel. Not used by the engine yet.
- <span id="i_MAX_CHANNELS"></span>**MAX_CHANNELS** = **8** --- Maximum number of channels a [VoxelBuffer](VoxelBuffer.md) can have.

enum **ChannelMask**: 

- <span id="i_CHANNEL_TYPE_BIT"></span>**CHANNEL_TYPE_BIT** = **1** --- Bitmask with one bit set at the position corresponding to [CHANNEL_TYPE](VoxelBuffer.md#i_CHANNEL_TYPE).
- <span id="i_CHANNEL_SDF_BIT"></span>**CHANNEL_SDF_BIT** = **2** --- Bitmask with one bit set at the position corresponding to [CHANNEL_SDF](VoxelBuffer.md#i_CHANNEL_SDF).
- <span id="i_CHANNEL_COLOR_BIT"></span>**CHANNEL_COLOR_BIT** = **4** --- Bitmask with one bit set at the position corresponding to [CHANNEL_COLOR](VoxelBuffer.md#i_CHANNEL_COLOR).
- <span id="i_CHANNEL_INDICES_BIT"></span>**CHANNEL_INDICES_BIT** = **8** --- Bitmask with one bit set at the position corresponding to [CHANNEL_INDICES](VoxelBuffer.md#i_CHANNEL_INDICES).
- <span id="i_CHANNEL_WEIGHTS_BIT"></span>**CHANNEL_WEIGHTS_BIT** = **16** --- Bitmask with one bit set at the position corresponding to [CHANNEL_WEIGHTS](VoxelBuffer.md#i_CHANNEL_WEIGHTS).
- <span id="i_CHANNEL_DATA5_BIT"></span>**CHANNEL_DATA5_BIT** = **32** --- Bitmask with one bit set at the position corresponding to [CHANNEL_DATA5](VoxelBuffer.md#i_CHANNEL_DATA5).
- <span id="i_CHANNEL_DATA6_BIT"></span>**CHANNEL_DATA6_BIT** = **64** --- Bitmask with one bit set at the position corresponding to [CHANNEL_DATA6](VoxelBuffer.md#i_CHANNEL_DATA6).
- <span id="i_CHANNEL_DATA7_BIT"></span>**CHANNEL_DATA7_BIT** = **128** --- Bitmask with one bit set at the position corresponding to [CHANNEL_DATA7](VoxelBuffer.md#i_CHANNEL_DATA7).
- <span id="i_ALL_CHANNELS_MASK"></span>**ALL_CHANNELS_MASK** = **255** --- Bitmask with all channel bits set.

enum **Depth**: 

- <span id="i_DEPTH_8_BIT"></span>**DEPTH_8_BIT** = **0** --- Voxels will be stored with 8 bits. Raw values will range from 0 to 255. Float values can take 255 values distributed from -10.0 to 10.0. Values outside the range will be clamped.
- <span id="i_DEPTH_16_BIT"></span>**DEPTH_16_BIT** = **1** --- Voxels will be stored with 16 bits. Raw values will range from 0 to 65,535. Float values can take 65,535 values distributed from -500.0 to 500.0. Values outside the range will be clamped.
- <span id="i_DEPTH_32_BIT"></span>**DEPTH_32_BIT** = **2** --- Voxels will be stored with 32 bits. Raw values will range from 0 to 4,294,967,295, and float values will use regular IEEE 754 representation (`float`).
- <span id="i_DEPTH_64_BIT"></span>**DEPTH_64_BIT** = **3** --- Voxels will be stored with 64 bits. Raw values will range from 0 to 18,446,744,073,709,551,615, and float values will use regular IEEE 754 representation (`double`).
- <span id="i_DEPTH_COUNT"></span>**DEPTH_COUNT** = **4** --- How many depth configuration there are.

enum **Compression**: 

- <span id="i_COMPRESSION_NONE"></span>**COMPRESSION_NONE** = **0** --- The channel is not compressed. Every value is stored individually inside an array in memory.
- <span id="i_COMPRESSION_UNIFORM"></span>**COMPRESSION_UNIFORM** = **1** --- All voxels of the channel have the same value, so they are stored as one single value, to save space.
- <span id="i_COMPRESSION_COUNT"></span>**COMPRESSION_COUNT** = **2** --- How many compression modes there are.

enum **Allocator**: 

- <span id="i_ALLOCATOR_DEFAULT"></span>**ALLOCATOR_DEFAULT** = **0** --- Uses Godot's default memory allocator (at time of writing, it is `malloc`). Preferred for occasional buffers with uncommon size, or very large size.
- <span id="i_ALLOCATOR_POOL"></span>**ALLOCATOR_POOL** = **1** --- Uses a pool allocator. Can be faster than the default allocator buffers are created very frequently with similar size. This memory will remain allocated after use, under the assumption that other buffers will need it soon after. Does not support very large buffers (greater than 2 megabytes)
- <span id="i_ALLOCATOR_COUNT"></span>**ALLOCATOR_COUNT** = **2**


## Constants: 

- <span id="i_MAX_SIZE"></span>**MAX_SIZE** = **65535** --- Maximum size a buffer can have when serialized. Buffers that contain uniform-compressed voxels can reach it, but in practice, the limit is much lower and depends on available memory.

## Method Descriptions

### [void](#)<span id="i_clear"></span> **clear**( ) 

Erases all contents of the buffer and resets its size to zero. Channel depths and default values are preserved.

### [void](#)<span id="i_clear_voxel_metadata"></span> **clear_voxel_metadata**( ) 

Erases all per-voxel metadata.

### [void](#)<span id="i_clear_voxel_metadata_in_area"></span> **clear_voxel_metadata_in_area**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) min_pos, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) max_pos ) 

Erases per-voxel metadata within the specified area.

### [void](#)<span id="i_compress_uniform_channels"></span> **compress_uniform_channels**( ) 

Finds channels that have the same value in all their voxels, and reduces memory usage by storing only one value instead. This is effective for example when large parts of the terrain are filled with air.

### [void](#)<span id="i_copy_channel_from"></span> **copy_channel_from**( [VoxelBuffer](VoxelBuffer.md) other, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) 

Copies all values from the channel of another [VoxelBuffer](VoxelBuffer.md) into the same channel for the current buffer. The depth formats must match.

### [void](#)<span id="i_copy_channel_from_area"></span> **copy_channel_from_area**( [VoxelBuffer](VoxelBuffer.md) other, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_min, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_max, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) dst_min, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) 

Copies values from a channel's sub-region of another [VoxelBuffer](VoxelBuffer.md) into the same channel for the current buffer, at a specific location. The depth formats must match.

If corners of the area represent a negative-size area, they will be sorted back.

If coordinates are entirely or partially out of bounds, they will be clipped automatically.

Copying across the same buffer to overlapping areas is not supported. You may use an intermediary buffer in this case.

### [void](#)<span id="i_copy_voxel_metadata_in_area"></span> **copy_voxel_metadata_in_area**( [VoxelBuffer](VoxelBuffer.md) src_buffer, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_min_pos, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_max_pos, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) dst_min_pos ) 

Copies per-voxel metadata from a sub-region of another [VoxelBuffer](VoxelBuffer.md) into the the current buffer, at a specific location. Values will be a shallow copy.

If corners of the area represent a negative-size area, they will be sorted back.

If coordinates are entirely or partially out of bounds, they will be clipped automatically.

Copying across the same buffer to overlapping areas is not supported. You may use an intermediary buffer in this case.

### [void](#)<span id="i_create"></span> **create**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) sx, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) sy, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) sz ) 

Clears the buffer and gives it the specified size.

### [ImageTexture3D](https://docs.godotengine.org/en/stable/classes/class_imagetexture3d.html)<span id="i_create_3d_texture_from_sdf_zxy"></span> **create_3d_texture_from_sdf_zxy**( [Format](https://docs.godotengine.org/en/stable/classes/class_image.html#enum-image-format) output_format ) 

Creates a 3D texture from the SDF channel.

If `output_format` is a 8-bit pixel format, the texture will contain normalized signed distances, where 0.5 is the isolevel, 0 is the furthest away under surface, and 1 is the furthest away above surface.

Only 16-bit SDF channel is supported.

Only [Image.FORMAT_R8](https://docs.godotengine.org/en/stable/classes/class_image.html#class-image-constant-FORMAT-R8) and [Image.FORMAT_L8](https://docs.godotengine.org/en/stable/classes/class_image.html#class-image-constant-FORMAT-L8) output formats are suported.

Note: when sampling this texture in a shader, you need to swizzle 3D coordinates with `.yxz`. This is how voxels are internally stored, and this function does not change this convention.

### [Image[]](https://docs.godotengine.org/en/stable/classes/class_image[].html)<span id="i_debug_print_sdf_y_slices"></span> **debug_print_sdf_y_slices**( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) scale=1.0 ) 

Renders the contents of the SDF channel into images where blue gradients are negative values (below surface) and yellow gradients are positive (above surface). Each image corresponds to an XZ slice of the buffer.

The `scale` parameter can be used to change contrast of images by scaling the SDF.

### [void](#)<span id="i_decompress_channel"></span> **decompress_channel**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) 

If the given channel is currently compressed, decompresses it so that all voxel values are individually stored in full. This will use more memory.

### [void](#)<span id="i_downscale_to"></span> **downscale_to**( [VoxelBuffer](VoxelBuffer.md) dst, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_min, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_max, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) dst_min ) 

Produces a downscaled version of this buffer, by a factor of 2, without any form of interpolation (i.e using nearest-neighbor).

Metadata is not copied.

### [void](#)<span id="i_fill"></span> **fill**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 

Fills one channel of this buffer with a specific raw value.

### [void](#)<span id="i_fill_area"></span> **fill_area**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) min, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) max, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 

Fills an area of a channel in this buffer with a specific raw value.

### [void](#)<span id="i_fill_area_f"></span> **fill_area_f**( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) value, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) min, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) max, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) 

Fills an area of a channel in this buffer with a specific SDF value.

### [void](#)<span id="i_fill_f"></span> **fill_f**( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 

Fills one channel of this buffer with a specific SDF value.

### [void](#)<span id="i_for_each_voxel_metadata"></span> **for_each_voxel_metadata**( [Callable](https://docs.godotengine.org/en/stable/classes/class_callable.html) callback ) 

Executes a function on every voxel in this buffer which have associated metadata.

The function's arguments must be (position: Vector3i, metadata: Variant).

IMPORTANT: inserting new or removing metadata from inside this function is not allowed.

### [void](#)<span id="i_for_each_voxel_metadata_in_area"></span> **for_each_voxel_metadata_in_area**( [Callable](https://docs.godotengine.org/en/stable/classes/class_callable.html) callback, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) min_pos, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) max_pos ) 

Executes a function on every voxel in this buffer which have associated metadata, within the specified area.

IMPORTANT: inserting new or removing metadata from inside this function is not allowed.

### [Allocator](VoxelBuffer.md#enumerations)<span id="i_get_allocator"></span> **get_allocator**( ) 

Gets which memory allocator is used by this buffer.

### [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)<span id="i_get_block_metadata"></span> **get_block_metadata**( ) 

Gets metadata associated to this [VoxelBuffer](VoxelBuffer.md).

### [PackedByteArray](https://docs.godotengine.org/en/stable/classes/class_packedbytearray.html)<span id="i_get_channel_as_byte_array"></span> **get_channel_as_byte_array**( [ChannelId](VoxelBuffer.md#enumerations) channel_index ) 

Gets voxel data from a channel as uncompressed raw bytes. Check [Depth](VoxelBuffer.md#enumerations) for information about the data format.

Note: if the channel is compressed, it will be decompressed on the fly into the returned array. If you want a different behavior in this case, check [get_channel_compression](VoxelBuffer.md#i_get_channel_compression) before calling this method.

### [Compression](VoxelBuffer.md#enumerations)<span id="i_get_channel_compression"></span> **get_channel_compression**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) 

Gets which compression mode the specified channel has.

### [Depth](VoxelBuffer.md#enumerations)<span id="i_get_channel_depth"></span> **get_channel_depth**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) 

Gets which bit depth the specified channel has.

### [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)<span id="i_get_size"></span> **get_size**( ) 

Gets the 3D size of the buffer in voxels.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_voxel"></span> **get_voxel**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 

Gets the raw value of a voxel within this buffer.

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_get_voxel_f"></span> **get_voxel_f**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 

Gets the float value of a voxel within this buffer. You may use this function if you work with SDF volumes (smooth voxels).

### [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)<span id="i_get_voxel_metadata"></span> **get_voxel_metadata**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos ) 

Gets the metadata attached to a specific voxel in this buffer.

### [VoxelTool](VoxelTool.md)<span id="i_get_voxel_tool"></span> **get_voxel_tool**( ) 

Constructs a [VoxelTool](VoxelTool.md) instance bound to this buffer. This provides access to some extra common functions.

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_is_uniform"></span> **is_uniform**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) 

Checks if every voxel within a channel has the same value.

### [void](#)<span id="i_mirror"></span> **mirror**( [Axis](https://docs.godotengine.org/en/stable/classes/class_vector3i.html#enum-vector3i-axis) axis ) 

Mirrors voxel values along the specified axis.

### [void](#)<span id="i_op_add_buffer_f"></span> **op_add_buffer_f**( [VoxelBuffer](VoxelBuffer.md) other, [ChannelId](VoxelBuffer.md#enumerations) channel ) 

Computes the sum of corresponding voxels between the current buffer and the other buffer, and stores the result in the current buffer. Voxels are interpreted as signed distances (usually the [CHANNEL_SDF](VoxelBuffer.md#i_CHANNEL_SDF) channel).

### [void](#)<span id="i_op_max_buffer_f"></span> **op_max_buffer_f**( [VoxelBuffer](VoxelBuffer.md) other, [ChannelId](VoxelBuffer.md#enumerations) channel ) 

Computes the maximum of corresponding voxels between the current buffer and the other buffer, and stores the result in the current buffer. Voxels are interpreted as signed distances (usually the [CHANNEL_SDF](VoxelBuffer.md#i_CHANNEL_SDF) channel).

### [void](#)<span id="i_op_min_buffer_f"></span> **op_min_buffer_f**( [VoxelBuffer](VoxelBuffer.md) other, [ChannelId](VoxelBuffer.md#enumerations) channel ) 

Computes the minimum of corresponding voxels between the current buffer and the other buffer, and stores the result in the current buffer. Voxels are interpreted as signed distances (usually the [CHANNEL_SDF](VoxelBuffer.md#i_CHANNEL_SDF) channel).

### [void](#)<span id="i_op_mul_buffer_f"></span> **op_mul_buffer_f**( [VoxelBuffer](VoxelBuffer.md) other, [ChannelId](VoxelBuffer.md#enumerations) channel ) 

Computes the multiplication of corresponding voxels between the current buffer and the other buffer, and stores the result in the current buffer. Voxels are interpreted as signed distances (usually the [CHANNEL_SDF](VoxelBuffer.md#i_CHANNEL_SDF) channel).

### [void](#)<span id="i_op_mul_value_f"></span> **op_mul_value_f**( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) other, [ChannelId](VoxelBuffer.md#enumerations) channel ) 

Multiplies every voxels in the buffer by the given value. Voxels are interpreted as signed distances (usually the [CHANNEL_SDF](VoxelBuffer.md#i_CHANNEL_SDF) channel).

### [void](#)<span id="i_op_select_less_src_f_dst_i_values"></span> **op_select_less_src_f_dst_i_values**( [VoxelBuffer](VoxelBuffer.md) src, [ChannelId](VoxelBuffer.md#enumerations) src_channel, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) threshold, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value_if_less, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value_if_more, [ChannelId](VoxelBuffer.md#enumerations) dst_channel ) 

For every voxel in the source buffer, if the value is lower than a threshold, set the corresponding voxel in the current buffer to a specific integer value, or another value otherwise. Voxels in the source buffer are interpreted as signed distances (usually the [CHANNEL_SDF](VoxelBuffer.md#i_CHANNEL_SDF) channel).

### [void](#)<span id="i_op_sub_buffer_f"></span> **op_sub_buffer_f**( [VoxelBuffer](VoxelBuffer.md) other, [ChannelId](VoxelBuffer.md#enumerations) channel ) 

Computes the subtraction of corresponding voxels between the current buffer and the other buffer (`current - other`), and stores the result in the current buffer. Voxels are interpreted as signed distances (usually the [CHANNEL_SDF](VoxelBuffer.md#i_CHANNEL_SDF) channel).

### [void](#)<span id="i_remap_values"></span> **remap_values**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel, [PackedInt32Array](https://docs.godotengine.org/en/stable/classes/class_packedint32array.html) map ) 

Remaps integer values in a channel using the passed `map` lookup table. Each index in `map` corresponds to an original value, and will be replaced by `map[original]`.

### [void](#)<span id="i_rotate_90"></span> **rotate_90**( [Axis](https://docs.godotengine.org/en/stable/classes/class_vector3i.html#enum-vector3i-axis) axis, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) turns ) 

Rotates voxel values by 90 degrees along the specified axis. 1 turn is 90 degrees, 2 turns is 180 degrees, 3 turns is 270 degrees (or -90), 4 turns does nothing. Negative turns go in the other direction. This may also rotate the size of the buffer, if it isn't cubic.

### [void](#)<span id="i_set_block_metadata"></span> **set_block_metadata**( [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) meta ) 

Sets arbitrary data on this buffer. Old data is replaced. Note, this is separate storage from per-voxel metadata.

If this [VoxelBuffer](VoxelBuffer.md) is saved, this metadata will also be saved along voxels, so make sure the data supports serialization (i.e you can't put nodes or arbitrary objects in it).

### [void](#)<span id="i_set_channel_depth"></span> **set_channel_depth**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel, [Depth](VoxelBuffer.md#enumerations) depth ) 

Changes the bit depth of a given channel. This controls the range of values a channel can hold. See [Depth](VoxelBuffer.md#enumerations) for more information.

### [void](#)<span id="i_set_channel_from_byte_array"></span> **set_channel_from_byte_array**( [ChannelId](VoxelBuffer.md#enumerations) channel_index, [PackedByteArray](https://docs.godotengine.org/en/stable/classes/class_packedbytearray.html) data ) 

Overwrites the contents of a channel from raw voxel data. Check [Depth](VoxelBuffer.md#enumerations) for information about the expected data format.

### [void](#)<span id="i_set_voxel"></span> **set_voxel**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 

Sets the raw value of a voxel. If you use smooth voxels, you may prefer using [set_voxel_f](VoxelBuffer.md#i_set_voxel_f).

### [void](#)<span id="i_set_voxel_f"></span> **set_voxel_f**( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 

Sets the float value of a voxel. This method should be used if you work on SDF data (smooth voxels).

### [void](#)<span id="i_set_voxel_metadata"></span> **set_voxel_metadata**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) value ) 

Attaches arbitrary data on a specific voxel. Old data is replaced. Passing `null` will erase metadata.

If this [VoxelBuffer](VoxelBuffer.md) is saved, this metadata will also be saved along voxels, so make sure the data supports serialization (i.e you can't put nodes or arbitrary objects in it).

### [void](#)<span id="i_set_voxel_v"></span> **set_voxel_v**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 

*(This method has no documentation)*

### [void](#)<span id="i_update_3d_texture_from_sdf_zxy"></span> **update_3d_texture_from_sdf_zxy**( [ImageTexture3D](https://docs.godotengine.org/en/stable/classes/class_imagetexture3d.html) existing_texture ) 

Updates an existing 3D texture from the SDF channel. See [create_3d_texture_from_sdf_zxy](VoxelBuffer.md#i_create_3d_texture_from_sdf_zxy) for more information.

_Generated on Aug 09, 2025_
