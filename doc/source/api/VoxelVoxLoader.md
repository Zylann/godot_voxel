# VoxelVoxLoader

Inherits: [RefCounted](https://docs.godotengine.org/en/stable/classes/class_refcounted.html)

Utility class to load voxels from MagicaVoxel files

## Methods: 


Return                                                                | Signature                                                                                                                                                                                                                                                                                        
--------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [load_from_file](#i_load_from_file) ( [String](https://docs.godotengine.org/en/stable/classes/class_string.html) fpath, [VoxelBuffer](VoxelBuffer.md) voxels, [VoxelColorPalette](VoxelColorPalette.md) palette, [ChannelId](VoxelBuffer.md#enumerations) dst_channel=CHANNEL_COLOR (2) ) static 
<p></p>

## Method Descriptions

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_load_from_file"></span> **load_from_file**( [String](https://docs.godotengine.org/en/stable/classes/class_string.html) fpath, [VoxelBuffer](VoxelBuffer.md) voxels, [VoxelColorPalette](VoxelColorPalette.md) palette, [ChannelId](VoxelBuffer.md#enumerations) dst_channel=CHANNEL_COLOR (2) ) 

Loads voxels from the first model found in a vox file and stores it in a single [VoxelBuffer](VoxelBuffer.md). Other models that might be in the file are not loaded. Transform of the model is not considered.

Data will be stored in one channel of the provided buffer. 64-bit depth channel is not supported.

If palette is provided, it will also load the color palette from the file and voxels will be indices pointing into it. 8-bit channel depth is optimal (as MagicaVoxel voxels use 8-bit indices), but 16-bit and 32-bit are also supported. 

If palette is not provided (null), the loader will attempt to store colors bit-packed directly into the voxels, according to the depth the provided buffer has (2-bit per component if 8-bit, 4-bit per component if 16-bit, 8-bit per component if 32-bit).

Returns an [Error](https://docs.godotengine.org/en/stable/classes/class_error.html) enum code to tell if loading succeeded or not.

Note: MagicaVoxel uses a different axis convention than Godot: X is right, Y is forwards and Z is up. Voxel coordinates will be the same when looked up in the buffer, but they mean different location in space.

_Generated on Aug 09, 2025_
