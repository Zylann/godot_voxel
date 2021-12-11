# VoxelBuffer

Inherits: [Reference](https://docs.godotengine.org/en/stable/classes/class_reference.html)


3D grid storing voxel data.

## Description: 

This contains dense voxels data storage (every single cell holds data, there is no sparse optimization of space). Works like a normal 3D grid containing a voxel value in each cell. Organized in channels of configurable bit depth. Values can be interpreted either as unsigned integers or normalized floats. See enum Depth for more information.

Arbitrary metadata can also be stored, either for the whole buffer, or per-voxel, at higher cost. This metadata can get saved and loaded along voxels, however you must make sure the data is serializable (i.e it should not contain nodes or arbitrary objects).

## Methods: 


Return                                                                        | Signature                                                                                                                                                                                                                                                                                                                                                                                                                                    
----------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                                     | [clear](#i_clear) ( )                                                                                                                                                                                                                                                                                                                                                                                                                        
[void](#)                                                                     | [clear_voxel_metadata](#i_clear_voxel_metadata) ( )                                                                                                                                                                                                                                                                                                                                                                                          
[void](#)                                                                     | [clear_voxel_metadata_in_area](#i_clear_voxel_metadata_in_area) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) min_pos, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) max_pos )                                                                                                                                                                                               
[void](#)                                                                     | [copy_channel_from](#i_copy_channel_from) ( [VoxelBuffer](VoxelBuffer.md) other, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel )                                                                                                                                                                                                                                                                              
[void](#)                                                                     | [copy_channel_from_area](#i_copy_channel_from_area) ( [VoxelBuffer](VoxelBuffer.md) other, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) src_min, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) src_max, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) dst_min, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel )  
[void](#)                                                                     | [copy_voxel_metadata_in_area](#i_copy_voxel_metadata_in_area) ( [VoxelBuffer](VoxelBuffer.md) src_buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) src_min_pos, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) src_max_pos, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) dst_min_pos )                                                     
[void](#)                                                                     | [create](#i_create) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) sx, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) sy, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) sz )                                                                                                                                                                                            
[void](#)                                                                     | [downscale_to](#i_downscale_to) ( [VoxelBuffer](VoxelBuffer.md) dst, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) src_min, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) src_max, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) dst_min ) const                                                                                                
[void](#)                                                                     | [fill](#i_fill) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 )                                                                                                                                                                                                                                                               
[void](#)                                                                     | [fill_area](#i_fill_area) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) min, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) max, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 )                                                                                 
[void](#)                                                                     | [fill_f](#i_fill_f) ( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 )                                                                                                                                                                                                                                                       
[void](#)                                                                     | [for_each_voxel_metadata](#i_for_each_voxel_metadata) ( [FuncRef](https://docs.godotengine.org/en/stable/classes/class_funcref.html) callback ) const                                                                                                                                                                                                                                                                                        
[void](#)                                                                     | [for_each_voxel_metadata_in_area](#i_for_each_voxel_metadata_in_area) ( [FuncRef](https://docs.godotengine.org/en/stable/classes/class_funcref.html) callback, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) min_pos, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) max_pos )                                                                                                  
[Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)  | [get_block_metadata](#i_get_block_metadata) ( ) const                                                                                                                                                                                                                                                                                                                                                                                        
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)          | [get_channel_compression](#i_get_channel_compression) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) const                                                                                                                                                                                                                                                                                                 
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)          | [get_channel_depth](#i_get_channel_depth) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) const                                                                                                                                                                                                                                                                                                             
[Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)  | [get_size](#i_get_size) ( ) const                                                                                                                                                                                                                                                                                                                                                                                                            
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)          | [get_size_x](#i_get_size_x) ( ) const                                                                                                                                                                                                                                                                                                                                                                                                        
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)          | [get_size_y](#i_get_size_y) ( ) const                                                                                                                                                                                                                                                                                                                                                                                                        
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)          | [get_size_z](#i_get_size_z) ( ) const                                                                                                                                                                                                                                                                                                                                                                                                        
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)          | [get_voxel](#i_get_voxel) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) const                                                                                                   
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)      | [get_voxel_f](#i_get_voxel_f) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) const                                                                                               
[Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)  | [get_voxel_metadata](#i_get_voxel_metadata) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos ) const                                                                                                                                                                                                                                                                                                       
[VoxelTool](VoxelTool.md)                                                     | [get_voxel_tool](#i_get_voxel_tool) ( )                                                                                                                                                                                                                                                                                                                                                                                                      
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)        | [is_uniform](#i_is_uniform) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) const                                                                                                                                                                                                                                                                                                                           
[void](#)                                                                     | [optimize](#i_optimize) ( )                                                                                                                                                                                                                                                                                                                                                                                                                  
[void](#)                                                                     | [set_block_metadata](#i_set_block_metadata) ( [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) meta )                                                                                                                                                                                                                                                                                                            
[void](#)                                                                     | [set_channel_depth](#i_set_channel_depth) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) depth )                                                                                                                                                                                                                                       
[void](#)                                                                     | [set_voxel](#i_set_voxel) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 )                             
[void](#)                                                                     | [set_voxel_f](#i_set_voxel_f) ( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 )                     
[void](#)                                                                     | [set_voxel_metadata](#i_set_voxel_metadata) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) value )                                                                                                                                                                                                                         
[void](#)                                                                     | [set_voxel_v](#i_set_voxel_v) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 )                                                                                                                                                               
<p></p>

## Enumerations: 

enum **ChannelId**: 

- **CHANNEL_TYPE** = **0** --- Channel used to store voxel types. Used by [VoxelMesherBlocky].
- **CHANNEL_SDF** = **1** --- Channel used to store SDF data (signed distance field). Used by [VoxelMesherTransvoxel] and other smooth meshers. Values should preferably be accessed as floats. Negative values are below the isosurface (inside matter), and positive values are above the surface (outside matter).
- **CHANNEL_COLOR** = **2** --- Channel used to store color data. Used by [VoxelMesherCubes].
- **CHANNEL_INDICES** = **3**
- **CHANNEL_WEIGHTS** = **4**
- **CHANNEL_DATA5** = **5** --- Free channel. Not used by the engine yet.
- **CHANNEL_DATA6** = **6** --- Free channel. Not used by the engine yet.
- **CHANNEL_DATA7** = **7** --- Free channel. Not used by the engine yet.
- **MAX_CHANNELS** = **8** --- Maximum number of channels a [VoxelBuffer] can have.

enum **Depth**: 

- **DEPTH_8_BIT** = **0** --- Voxels will be stored with 8 bits. Raw values will range from 0 to 255, and float values will be normalized between -1 and 1 (but will still take 255 possible values). Values outside the range will be clamped. If you use this for smooth voxels, you may take care of scaling SDF data with a small number like 0.1 to reduce precision artifacts.
- **DEPTH_16_BIT** = **1** --- Voxels will be stored with 16 bits. Raw values will range from 0 to 65,535, and float values will be normalized between -1 and 1 (but will still take 65535 possible values). Values outside the range will be clamped.
- **DEPTH_32_BIT** = **2** --- Voxels will be stored with 32 bits. Raw values will range from 0 to 4,294,967,295, and float values will use regular IEEE 754 representation (`float`).
- **DEPTH_64_BIT** = **3** --- Voxels will be stored with 64 bits. Raw values will range from 0 to 18,446,744,073,709,551,615, and float values will use regular IEEE 754 representation (`double`).
- **DEPTH_COUNT** = **4** --- How many depth configuration there are.

enum **Compression**: 

- **COMPRESSION_NONE** = **0** --- The channel is not compressed. Every value is stored individually inside an array in memory.
- **COMPRESSION_UNIFORM** = **1** --- All voxels of the channel have the same value, so they are stored as one single value, to save space.
- **COMPRESSION_COUNT** = **2** --- How many compression modes there are.


## Constants: 

- **MAX_SIZE** = **65535**

## Method Descriptions

- [void](#)<span id="i_clear"></span> **clear**( ) 

Erases all contents of the buffer and resets its size to zero. Channel depths and default values are preserved.

- [void](#)<span id="i_clear_voxel_metadata"></span> **clear_voxel_metadata**( ) 

Erases all per-voxel metadata.

- [void](#)<span id="i_clear_voxel_metadata_in_area"></span> **clear_voxel_metadata_in_area**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) min_pos, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) max_pos ) 

Erases per-voxel metadata within the specified area.

- [void](#)<span id="i_copy_channel_from"></span> **copy_channel_from**( [VoxelBuffer](VoxelBuffer.md) other, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) 

Copies all values from the channel of another [VoxelBuffer](VoxelBuffer.md) into the same channel for the current buffer. The depth formats must match.

- [void](#)<span id="i_copy_channel_from_area"></span> **copy_channel_from_area**( [VoxelBuffer](VoxelBuffer.md) other, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) src_min, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) src_max, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) dst_min, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) 

Copies values from a channel's sub-region of another [VoxelBuffer](VoxelBuffer.md) into the same channel for the current buffer, at a specific location. The depth formats must match.

If corners of the area represent a negative-size area, they will be sorted back.

If coordinates are entirely or partially out of bounds, they will be clipped automatically.

Copying across the same buffer to overlapping areas is not supported. You may use an intermediary buffer in this case.

- [void](#)<span id="i_copy_voxel_metadata_in_area"></span> **copy_voxel_metadata_in_area**( [VoxelBuffer](VoxelBuffer.md) src_buffer, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) src_min_pos, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) src_max_pos, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) dst_min_pos ) 

Copies per-voxel metadata from a sub-region of another [VoxelBuffer](VoxelBuffer.md) into the the current buffer, at a specific location. Values will be a shallow copy.

If corners of the area represent a negative-size area, they will be sorted back.

If coordinates are entirely or partially out of bounds, they will be clipped automatically.

Copying across the same buffer to overlapping areas is not supported. You may use an intermediary buffer in this case.

- [void](#)<span id="i_create"></span> **create**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) sx, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) sy, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) sz ) 

Clears the buffer and gives it the specified size.

- [void](#)<span id="i_downscale_to"></span> **downscale_to**( [VoxelBuffer](VoxelBuffer.md) dst, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) src_min, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) src_max, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) dst_min ) 

Produces a downscaled version of this buffer, by a factor of 2, without any form of interpolation (i.e using nearest-neighbor).

Metadata is not copied.

- [void](#)<span id="i_fill"></span> **fill**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 

Fills one channel of this buffer with a specific raw value.

- [void](#)<span id="i_fill_area"></span> **fill_area**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) min, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) max, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 

Fills an area of a channel in this buffer with a specific raw value.

- [void](#)<span id="i_fill_f"></span> **fill_f**( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 

Fills one channel of this buffer with a specific float value.

- [void](#)<span id="i_for_each_voxel_metadata"></span> **for_each_voxel_metadata**( [FuncRef](https://docs.godotengine.org/en/stable/classes/class_funcref.html) callback ) 

Executes a function on every voxel in this buffer which have associated metadata.

- [void](#)<span id="i_for_each_voxel_metadata_in_area"></span> **for_each_voxel_metadata_in_area**( [FuncRef](https://docs.godotengine.org/en/stable/classes/class_funcref.html) callback, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) min_pos, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) max_pos ) 

Executes a function on every voxel in this buffer which have associated metadata, within the specified area.

- [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)<span id="i_get_block_metadata"></span> **get_block_metadata**( ) 

Gets metadata associated to this [VoxelBuffer](VoxelBuffer.md).

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_channel_compression"></span> **get_channel_compression**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) 

Gets which compression mode the specified channel has.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_channel_depth"></span> **get_channel_depth**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) 

Gets which bit depth the specified channel has.

- [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)<span id="i_get_size"></span> **get_size**( ) 

Gets the 3D size of the buffer in voxels.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_size_x"></span> **get_size_x**( ) 

Gets how many voxels the buffer contains across the X axis.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_size_y"></span> **get_size_y**( ) 

Gets how many voxels the buffer contains across the Y axis.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_size_z"></span> **get_size_z**( ) 

Gets how many voxels the buffer contains across the Z axis.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_voxel"></span> **get_voxel**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 

Gets the raw value of a voxel within this buffer.

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_get_voxel_f"></span> **get_voxel_f**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 

Gets the float value of a voxel within this buffer. You may use this function if you work with SDF volumes (smooth voxels).

- [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html)<span id="i_get_voxel_metadata"></span> **get_voxel_metadata**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos ) 

Gets the metadata attached to a specific voxel in this buffer.

- [VoxelTool](VoxelTool.md)<span id="i_get_voxel_tool"></span> **get_voxel_tool**( ) 

Constructs a [VoxelTool](VoxelTool.md) instance bound to this buffer. This provides access to some extra common functions.

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_is_uniform"></span> **is_uniform**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel ) 

Checks if every voxel within a channel has the same value.

- [void](#)<span id="i_optimize"></span> **optimize**( ) 


- [void](#)<span id="i_set_block_metadata"></span> **set_block_metadata**( [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) meta ) 

Sets arbitrary data on this buffer. Old data is replaced. Note, this is separate storage from per-voxel metadata.

If this [VoxelBuffer](VoxelBuffer.md) is saved, this metadata will also be saved along voxels, so make sure the data supports serialization (i.e you can't put nodes or arbitrary objects in it).

- [void](#)<span id="i_set_channel_depth"></span> **set_channel_depth**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) depth ) 

Changes the bit depth of a given channel. This controls the range of values a channel can hold. See enum VoxelBuffer.Depth for more information.

- [void](#)<span id="i_set_voxel"></span> **set_voxel**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 

Sets the raw value of a voxel. If you use smooth voxels, you may prefer using method set_voxel_f.

- [void](#)<span id="i_set_voxel_f"></span> **set_voxel_f**( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) value, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) x, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) y, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) z, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 

Sets the float value of a voxel. This method should be used if you work on SDF data (smooth voxels).

- [void](#)<span id="i_set_voxel_metadata"></span> **set_voxel_metadata**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) value ) 

Attaches arbitrary data on a specific voxel. Old data is replaced.

If this [VoxelBuffer](VoxelBuffer.md) is saved, this metadata will also be saved along voxels, so make sure the data supports serialization (i.e you can't put nodes or arbitrary objects in it).

- [void](#)<span id="i_set_voxel_v"></span> **set_voxel_v**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) value, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel=0 ) 


_Generated on Nov 06, 2021_
