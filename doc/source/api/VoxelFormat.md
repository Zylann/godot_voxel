# VoxelFormat

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)

Specifies the format of voxels.

## Description: 

Specifies the format of voxels. Currently, it only stores how many bytes each channel uses per voxel.

Voxels have a default format which is often enough for most use cases, but sometimes it is necessary to change it. In this case, you may create a new [VoxelFormat](VoxelFormat.md) resource, do the changes, and assign it to a [VoxelNode](VoxelNode.md).

WARNING: it is recommended to choose a format early in development (whether it is the default, or a custom one). If you want to change much later and you have saves in the wild, you will have to figure out how to convert them, otherwise loading them will be problematic.

## Properties: 


Type                                                                      | Name                               | Default                     
------------------------------------------------------------------------- | ---------------------------------- | ----------------------------
[Array](https://docs.godotengine.org/en/stable/classes/class_array.html)  | [_data](#i__data)                  | [0, 1, 1, 0, 1, 1, 0, 0, 0] 
[Depth](VoxelBuffer.md#enumerations)                                      | [color_depth](#i_color_depth)      | DEPTH_8_BIT (0)             
[Depth](VoxelBuffer.md#enumerations)                                      | [indices_depth](#i_indices_depth)  | DEPTH_16_BIT (1)            
[Depth](VoxelBuffer.md#enumerations)                                      | [sdf_depth](#i_sdf_depth)          | DEPTH_16_BIT (1)            
[Depth](VoxelBuffer.md#enumerations)                                      | [type_depth](#i_type_depth)        | DEPTH_16_BIT (1)            
<p></p>

## Methods: 


Return                                | Signature                                                                                                                                         
------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                             | [configure_buffer](#i_configure_buffer) ( [VoxelBuffer](VoxelBuffer.md) buffer ) const                                                            
[VoxelBuffer](VoxelBuffer.md)         | [create_buffer](#i_create_buffer) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) size ) const                   
[Depth](VoxelBuffer.md#enumerations)  | [get_channel_depth](#i_get_channel_depth) ( [ChannelId](VoxelBuffer.md#enumerations) channel_index ) const                                        
[void](#)                             | [set_channel_depth](#i_set_channel_depth) ( [ChannelId](VoxelBuffer.md#enumerations) channel_index, [Depth](VoxelBuffer.md#enumerations) depth )  
<p></p>

## Property Descriptions

### [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i__data"></span> **_data** = [0, 1, 1, 0, 1, 1, 0, 0, 0]

*(This property has no documentation)*

### [Depth](VoxelBuffer.md#enumerations)<span id="i_color_depth"></span> **color_depth** = DEPTH_8_BIT (0)

Depth of [VoxelBuffer.CHANNEL_COLOR](VoxelBuffer.md#i_CHANNEL_COLOR).

### [Depth](VoxelBuffer.md#enumerations)<span id="i_indices_depth"></span> **indices_depth** = DEPTH_16_BIT (1)

Depth of [VoxelBuffer.CHANNEL_INDICES](VoxelBuffer.md#i_CHANNEL_INDICES). Only 8-bit and 16-bit depths are supported.

### [Depth](VoxelBuffer.md#enumerations)<span id="i_sdf_depth"></span> **sdf_depth** = DEPTH_16_BIT (1)

Depth of [VoxelBuffer.CHANNEL_SDF](VoxelBuffer.md#i_CHANNEL_SDF).

### [Depth](VoxelBuffer.md#enumerations)<span id="i_type_depth"></span> **type_depth** = DEPTH_16_BIT (1)

Depth of [VoxelBuffer.CHANNEL_TYPE](VoxelBuffer.md#i_CHANNEL_TYPE). Only 8-bit and 16-bit depths are supported.

## Method Descriptions

### [void](#)<span id="i_configure_buffer"></span> **configure_buffer**( [VoxelBuffer](VoxelBuffer.md) buffer ) 

Clears and formats the [VoxelBuffer](VoxelBuffer.md) using properties from the current format. Should be used on a buffer that hasn't been modified yet.

### [VoxelBuffer](VoxelBuffer.md)<span id="i_create_buffer"></span> **create_buffer**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) size ) 

Creates a new [VoxelBuffer](VoxelBuffer.md) that has the current format.

### [Depth](VoxelBuffer.md#enumerations)<span id="i_get_channel_depth"></span> **get_channel_depth**( [ChannelId](VoxelBuffer.md#enumerations) channel_index ) 

Gets the depth of a specific channel. See [Depth](VoxelBuffer.md#enumerations) for more information.

### [void](#)<span id="i_set_channel_depth"></span> **set_channel_depth**( [ChannelId](VoxelBuffer.md#enumerations) channel_index, [Depth](VoxelBuffer.md#enumerations) depth ) 

Sets the depth of a specific channel. See [Depth](VoxelBuffer.md#enumerations) for more information.

_Generated on Aug 09, 2025_
