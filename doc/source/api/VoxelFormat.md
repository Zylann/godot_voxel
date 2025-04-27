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
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)      | [color_depth](#i_color_depth)      | 0                           
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)      | [indices_depth](#i_indices_depth)  | 1                           
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)      | [sdf_depth](#i_sdf_depth)          | 1                           
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)      | [type_depth](#i_type_depth)        | 1                           
<p></p>

## Methods: 


Return                                                                | Signature                                                                                                                                                                                                     
--------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                             | [configure_buffer](#i_configure_buffer) ( [VoxelBuffer](VoxelBuffer.md) buffer ) const                                                                                                                        
[void](#)                                                             | [create_buffer](#i_create_buffer) ( [VoxelBuffer](VoxelBuffer.md) size ) const                                                                                                                                
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [get_channel_depth](#i_get_channel_depth) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel_index ) const                                                                        
[void](#)                                                             | [set_channel_depth](#i_set_channel_depth) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) depth )  
<p></p>

## Property Descriptions

### [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i__data"></span> **_data** = [0, 1, 1, 0, 1, 1, 0, 0, 0]

*(This property has no documentation)*

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_color_depth"></span> **color_depth** = 0

Depth of [VoxelBuffer.CHANNEL_COLOR](VoxelBuffer.md#i_CHANNEL_COLOR).

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_indices_depth"></span> **indices_depth** = 1

Depth of [VoxelBuffer.CHANNEL_INDICES](VoxelBuffer.md#i_CHANNEL_INDICES). Only 8-bit and 16-bit depths are supported.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_sdf_depth"></span> **sdf_depth** = 1

Depth of [VoxelBuffer.CHANNEL_SDF](VoxelBuffer.md#i_CHANNEL_SDF).

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_type_depth"></span> **type_depth** = 1

Depth of [VoxelBuffer.CHANNEL_TYPE](VoxelBuffer.md#i_CHANNEL_TYPE). Only 8-bit and 16-bit depths are supported.

## Method Descriptions

### [void](#)<span id="i_configure_buffer"></span> **configure_buffer**( [VoxelBuffer](VoxelBuffer.md) buffer ) 

Clears and formats the [VoxelBuffer](VoxelBuffer.md) using properties from the current format. Should be used on a buffer that hasn't been modified yet.

### [void](#)<span id="i_create_buffer"></span> **create_buffer**( [VoxelBuffer](VoxelBuffer.md) size ) 

Creates a new [VoxelBuffer](VoxelBuffer.md) that has the current format.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_channel_depth"></span> **get_channel_depth**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel_index ) 

Gets the depth of a specific channel. See [VoxelBuffer.Depth](VoxelBuffer.md#enumerations) for more information.

### [void](#)<span id="i_set_channel_depth"></span> **set_channel_depth**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) channel_index, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) depth ) 

Sets the depth of a specific channel. See [VoxelBuffer.Depth](VoxelBuffer.md#enumerations) for more information.

_Generated on Apr 27, 2025_
