# VoxelBuffer

Inherits: [RefCounted](https://docs.godotengine.org/en/stable/classes/class_refcounted.html)

3D grid storing voxel data.

## Description: 

This contains dense voxels data storage (every single cell holds data, there is no sparse optimization of space). Works like a normal 3D grid containing a voxel value in each cell. Organized in channels of configurable bit depth. Values can be interpreted either as unsigned integers or normalized floats. See [VoxelBuffer.Depth](VoxelBuffer.md#enumerations) for more information.

Arbitrary metadata can also be stored, either for the whole buffer, or per-voxel, at higher cost. This metadata can get saved and loaded along voxels, however you must make sure the data is serializable (i.e it should not contain nodes or arbitrary objects).

## Methods: 


Return                                                                          | Signature                                                                                                                                                                                                                                                                                                                                                                                                                                          
------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                                       | [clear](#i_clear) ( )                                                                                                                                                                                                                                                                                                                                                                                                                              
[void](#)                                                                       | [clear_voxel_metadata](#i_clear_voxel_metadata) ( )                                                                                                                                                                                                                                                                                                                                                                                                
[void](#)                                                                       | [clear_voxel_metadata_in_area](#i_clear_voxel_metadata_in_area) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) min_pos, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) max_pos )                                                                                                                                                                                                 
[void](#)                                                                       | [compress_uniform_channels](#i_compress_uniform_channels) ( )                                                                                                                                                                                                                                                                                                                                                                                      
[void](#)                                                                       | [copy_channel_from](#i_copy_channel_from) ( [VoxelBuffer](VoxelBuffer.md) other, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel )                                                                                                                                                                                                                                                                                    
[void](#)                                                                       | [copy_channel_from_area](#i_copy_channel_from_area) ( [VoxelBuffer](VoxelBuffer.md) other, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_min, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_max, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) dst_min, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel )  
[void](#)                                                                       | [copy_voxel_metadata_in_area](#i_copy_voxel_metadata_in_area) ( [VoxelBuffer](VoxelBuffer.md) src_buffer, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_min_pos, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_max_pos, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) dst_min_pos )                                                     
[void](#)                                                                       | [create](#i_create) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) sx, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) sy, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) sz )                                                                                                                                                                                                  
[Array](https://docs.godotengine.org/en/stable/classes/class_array.html)        | [debug_print_sdf_y_slices](#i_debug_print_sdf_y_slices) ( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) scale ) const                                                                                                                                                                                                                                                                                                   
[void](#)                                                                       | [downscale_to](#i_downscale_to) ( [VoxelBuffer](VoxelBuffer.md) dst, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_min, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_max, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) dst_min ) const                                                                                                
[void](#)                                                                       | [fill](#i_fill) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 )                                                                                                                                                                                                                                                                     
[void](#)                                                                       | [fill_area](#i_fill_area) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) min, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) max, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 )                                                                                   
[void](#)                                                                       | [fill_f](#i_fill_f) ( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 )                                                                                                                                                                                                                                                             
[void](#)                                                                       | [for_each_voxel_metadata](#i_for_each_voxel_metadata) ( [Callable](https://docs.godotengine.org/en/stable/classes/class_callable.html) callback ) const                                                                                                                                                                                                                                                                                            
[void](#)                                                                       | [for_each_voxel_metadata_in_area](#i_for_each_voxel_metadata_in_area) ( [Callable](https://docs.godotengine.org/en/stable/classes/class_callable.html) callback, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) min_pos, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) max_pos )                                                                                                  
[Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)    | [get_block_metadata](#i_get_block_metadata) ( ) const                                                                                                                                                                                                                                                                                                                                                                                              
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)            | [get_channel_compression](#i_get_channel_compression) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) const                                                                                                                                                                                                                                                                                                       
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)            | [get_channel_depth](#i_get_channel_depth) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) const                                                                                                                                                                                                                                                                                                                   
[Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)  | [get_size](#i_get_size) ( ) const                                                                                                                                                                                                                                                                                                                                                                                                                  
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)            | [get_voxel](#i_get_voxel) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) const                                                                                                         
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)        | [get_voxel_f](#i_get_voxel_f) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) const                                                                                                     
[Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)    | [get_voxel_metadata](#i_get_voxel_metadata) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos ) const                                                                                                                                                                                                                                                                                                           
[VoxelTool](VoxelTool.md)                                                       | [get_voxel_tool](#i_get_voxel_tool) ( )                                                                                                                                                                                                                                                                                                                                                                                                            
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)          | [is_uniform](#i_is_uniform) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) const                                                                                                                                                                                                                                                                                                                                 
[void](#)                                                                       | [optimize](#i_optimize) ( )                                                                                                                                                                                                                                                                                                                                                                                                                        
[void](#)                                                                       | [remap_values](#i_remap_values) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel, [PackedInt32Array](https://docs.godotengine.org/en/stable/classes/class_packedint32array.html) map )                                                                                                                                                                                                                               
[void](#)                                                                       | [set_block_metadata](#i_set_block_metadata) ( [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) meta )                                                                                                                                                                                                                                                                                                                  
[void](#)                                                                       | [set_channel_depth](#i_set_channel_depth) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) depth )                                                                                                                                                                                                                                             
[void](#)                                                                       | [set_voxel](#i_set_voxel) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 )                                   
[void](#)                                                                       | [set_voxel_f](#i_set_voxel_f) ( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 )                           
[void](#)                                                                       | [set_voxel_metadata](#i_set_voxel_metadata) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) value )                                                                                                                                                                                                                             
[void](#)                                                                       | [set_voxel_v](#i_set_voxel_v) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 )                                                                                                                                                                   
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

enum **Depth**: 

- <span id="i_DEPTH_8_BIT"></span>**DEPTH_8_BIT** = **0** --- Voxels will be stored with 8 bits. Raw values will range from 0 to 255, and float values will be normalized between -1 and 1 (but will still take 255 possible values). Values outside the range will be clamped. If you use this for smooth voxels, you may take care of scaling SDF data with a small number like 0.1 to reduce precision artifacts.
- <span id="i_DEPTH_16_BIT"></span>**DEPTH_16_BIT** = **1** --- Voxels will be stored with 16 bits. Raw values will range from 0 to 65,535, and float values will be normalized between -1 and 1 (but will still take 65535 possible values). Values outside the range will be clamped.
- <span id="i_DEPTH_32_BIT"></span>**DEPTH_32_BIT** = **2** --- Voxels will be stored with 32 bits. Raw values will range from 0 to 4,294,967,295, and float values will use regular IEEE 754 representation (`float`).
- <span id="i_DEPTH_64_BIT"></span>**DEPTH_64_BIT** = **3** --- Voxels will be stored with 64 bits. Raw values will range from 0 to 18,446,744,073,709,551,615, and float values will use regular IEEE 754 representation (`double`).
- <span id="i_DEPTH_COUNT"></span>**DEPTH_COUNT** = **4** --- How many depth configuration there are.

enum **Compression**: 

- <span id="i_COMPRESSION_NONE"></span>**COMPRESSION_NONE** = **0** --- The channel is not compressed. Every value is stored individually inside an array in memory.
- <span id="i_COMPRESSION_UNIFORM"></span>**COMPRESSION_UNIFORM** = **1** --- All voxels of the channel have the same value, so they are stored as one single value, to save space.
- <span id="i_COMPRESSION_COUNT"></span>**COMPRESSION_COUNT** = **2** --- How many compression modes there are.


## Constants: 

- <span id="i_MAX_SIZE"></span>**MAX_SIZE** = **65535**

## Method Descriptions

- [void](#)<span id="i_clear"></span> **clear**( ) 

Erases all contents of the buffer and resets its size to zero. Channel depths and default values are preserved.

- [void](#)<span id="i_clear_voxel_metadata"></span> **clear_voxel_metadata**( ) 

Erases all per-voxel metadata.

- [void](#)<span id="i_clear_voxel_metadata_in_area"></span> **clear_voxel_metadata_in_area**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) min_pos, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) max_pos ) 

Erases per-voxel metadata within the specified area.

- [void](#)<span id="i_compress_uniform_channels"></span> **compress_uniform_channels**( ) 

Finds channels that have the same value in all their voxels, and reduces memory usage by storing only one value instead. This is effective for example when large parts of the terrain are filled with air.

- [void](#)<span id="i_copy_channel_from"></span> **copy_channel_from**( [VoxelBuffer](VoxelBuffer.md) other, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) 

Copies all values from the channel of another [VoxelBuffer](VoxelBuffer.md) into the same channel for the current buffer. The depth formats must match.

- [void](#)<span id="i_copy_channel_from_area"></span> **copy_channel_from_area**( [VoxelBuffer](VoxelBuffer.md) other, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_min, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_max, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) dst_min, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) 

Copies values from a channel's sub-region of another [VoxelBuffer](VoxelBuffer.md) into the same channel for the current buffer, at a specific location. The depth formats must match.

If corners of the area represent a negative-size area, they will be sorted back.

If coordinates are entirely or partially out of bounds, they will be clipped automatically.

Copying across the same buffer to overlapping areas is not supported. You may use an intermediary buffer in this case.

- [void](#)<span id="i_copy_voxel_metadata_in_area"></span> **copy_voxel_metadata_in_area**( [VoxelBuffer](VoxelBuffer.md) src_buffer, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_min_pos, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_max_pos, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) dst_min_pos ) 

Copies per-voxel metadata from a sub-region of another [VoxelBuffer](VoxelBuffer.md) into the the current buffer, at a specific location. Values will be a shallow copy.

If corners of the area represent a negative-size area, they will be sorted back.

If coordinates are entirely or partially out of bounds, they will be clipped automatically.

Copying across the same buffer to overlapping areas is not supported. You may use an intermediary buffer in this case.

- [void](#)<span id="i_create"></span> **create**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) sx, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) sy, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) sz ) 

Clears the buffer and gives it the specified size.

- [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_debug_print_sdf_y_slices"></span> **debug_print_sdf_y_slices**( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) scale ) 


- [void](#)<span id="i_downscale_to"></span> **downscale_to**( [VoxelBuffer](VoxelBuffer.md) dst, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_min, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) src_max, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) dst_min ) 

Produces a downscaled version of this buffer, by a factor of 2, without any form of interpolation (i.e using nearest-neighbor).

Metadata is not copied.

- [void](#)<span id="i_fill"></span> **fill**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 

Fills one channel of this buffer with a specific raw value.

- [void](#)<span id="i_fill_area"></span> **fill_area**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) min, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) max, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 

Fills an area of a channel in this buffer with a specific raw value.

- [void](#)<span id="i_fill_f"></span> **fill_f**( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 

Fills one channel of this buffer with a specific float value.

- [void](#)<span id="i_for_each_voxel_metadata"></span> **for_each_voxel_metadata**( [Callable](https://docs.godotengine.org/en/stable/classes/class_callable.html) callback ) 

Executes a function on every voxel in this buffer which have associated metadata.

The function's arguments must be (position: Vector3i, metadata: Variant).

IMPORTANT: inserting new or removing metadata from inside this function is not allowed.

- [void](#)<span id="i_for_each_voxel_metadata_in_area"></span> **for_each_voxel_metadata_in_area**( [Callable](https://docs.godotengine.org/en/stable/classes/class_callable.html) callback, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) min_pos, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) max_pos ) 

Executes a function on every voxel in this buffer which have associated metadata, within the specified area.

- [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)<span id="i_get_block_metadata"></span> **get_block_metadata**( ) 

Gets metadata associated to this [VoxelBuffer](VoxelBuffer.md).

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_channel_compression"></span> **get_channel_compression**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) 

Gets which compression mode the specified channel has.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_channel_depth"></span> **get_channel_depth**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) 

Gets which bit depth the specified channel has.

- [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)<span id="i_get_size"></span> **get_size**( ) 

Gets the 3D size of the buffer in voxels.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_voxel"></span> **get_voxel**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 

Gets the raw value of a voxel within this buffer.

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_get_voxel_f"></span> **get_voxel_f**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 

Gets the float value of a voxel within this buffer. You may use this function if you work with SDF volumes (smooth voxels).

- [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)<span id="i_get_voxel_metadata"></span> **get_voxel_metadata**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos ) 

Gets the metadata attached to a specific voxel in this buffer.

- [VoxelTool](VoxelTool.md)<span id="i_get_voxel_tool"></span> **get_voxel_tool**( ) 

Constructs a [VoxelTool](VoxelTool.md) instance bound to this buffer. This provides access to some extra common functions.

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_is_uniform"></span> **is_uniform**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) 

Checks if every voxel within a channel has the same value.

- [void](#)<span id="i_optimize"></span> **optimize**( ) 


- [void](#)<span id="i_remap_values"></span> **remap_values**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel, [PackedInt32Array](https://docs.godotengine.org/en/stable/classes/class_packedint32array.html) map ) 


- [void](#)<span id="i_set_block_metadata"></span> **set_block_metadata**( [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) meta ) 

Sets arbitrary data on this buffer. Old data is replaced. Note, this is separate storage from per-voxel metadata.

If this [VoxelBuffer](VoxelBuffer.md) is saved, this metadata will also be saved along voxels, so make sure the data supports serialization (i.e you can't put nodes or arbitrary objects in it).

- [void](#)<span id="i_set_channel_depth"></span> **set_channel_depth**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) depth ) 

Changes the bit depth of a given channel. This controls the range of values a channel can hold. See [VoxelBuffer.Depth](VoxelBuffer.md#enumerations) for more information.

- [void](#)<span id="i_set_voxel"></span> **set_voxel**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 

Sets the raw value of a voxel. If you use smooth voxels, you may prefer using [VoxelBuffer.set_voxel_f](VoxelBuffer.md#i_set_voxel_f).

- [void](#)<span id="i_set_voxel_f"></span> **set_voxel_f**( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 

Sets the float value of a voxel. This method should be used if you work on SDF data (smooth voxels).

- [void](#)<span id="i_set_voxel_metadata"></span> **set_voxel_metadata**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) value ) 

Attaches arbitrary data on a specific voxel. Old data is replaced.

If this [VoxelBuffer](VoxelBuffer.md) is saved, this metadata will also be saved along voxels, so make sure the data supports serialization (i.e you can't put nodes or arbitrary objects in it).

- [void](#)<span id="i_set_voxel_v"></span> **set_voxel_v**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) pos, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 


_Generated on Nov 11, 2023_
