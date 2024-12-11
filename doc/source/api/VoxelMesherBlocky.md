# VoxelMesherBlocky

Inherits: [VoxelMesher](VoxelMesher.md)

Produces a mesh by batching models corresponding to each voxel value, similar to games like Minecraft or StarMade.

## Description: 

Occluded faces are removed from the result, and some degree of ambient occlusion can be baked on the edges. Values are expected to be in the [VoxelBuffer.CHANNEL_TYPE](VoxelBuffer.md#i_CHANNEL_TYPE) channel. Models are defined with a [VoxelBlockyLibrary](VoxelBlockyLibrary.md), in which model indices correspond to the voxel values. Models don't have to be cubes.

## Properties: 


Type                                                                      | Name                                                         | Default 
------------------------------------------------------------------------- | ------------------------------------------------------------ | --------
[VoxelBlockyLibraryBase](VoxelBlockyLibraryBase.md)                       | [library](#i_library)                                        |         
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [occlusion_darkness](#i_occlusion_darkness)                  | 0.8     
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)    | [occlusion_enabled](#i_occlusion_enabled)                    | true    
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)    | [shadow_occluder_negative_x](#i_shadow_occluder_negative_x)  | false   
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)    | [shadow_occluder_negative_y](#i_shadow_occluder_negative_y)  | false   
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)    | [shadow_occluder_negative_z](#i_shadow_occluder_negative_z)  | false   
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)    | [shadow_occluder_positive_x](#i_shadow_occluder_positive_x)  | false   
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)    | [shadow_occluder_positive_y](#i_shadow_occluder_positive_y)  | false   
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)    | [shadow_occluder_positive_z](#i_shadow_occluder_positive_z)  | false   
<p></p>

## Methods: 


Return                                                                  | Signature                                                                                                                                                                                                              
----------------------------------------------------------------------- | -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)  | [get_shadow_occluder_side](#i_get_shadow_occluder_side) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) side ) const                                                                            
[void](#)                                                               | [set_shadow_occluder_side](#i_set_shadow_occluder_side) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) side, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled )  
<p></p>

## Enumerations: 

enum **Side**: 

- <span id="i_SIDE_NEGATIVE_X"></span>**SIDE_NEGATIVE_X** = **0**
- <span id="i_SIDE_POSITIVE_X"></span>**SIDE_POSITIVE_X** = **1**
- <span id="i_SIDE_NEGATIVE_Y"></span>**SIDE_NEGATIVE_Y** = **2**
- <span id="i_SIDE_POSITIVE_Y"></span>**SIDE_POSITIVE_Y** = **3**
- <span id="i_SIDE_NEGATIVE_Z"></span>**SIDE_NEGATIVE_Z** = **4**
- <span id="i_SIDE_POSITIVE_Z"></span>**SIDE_POSITIVE_Z** = **5**


## Property Descriptions

### [VoxelBlockyLibraryBase](VoxelBlockyLibraryBase.md)<span id="i_library"></span> **library**

Library of models that will be used by this mesher. If you are using a mesher without a terrain, make sure you call [VoxelBlockyLibraryBase.bake](VoxelBlockyLibraryBase.md#i_bake) before building meshes, otherwise results will be empty or out-of-date.

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_occlusion_darkness"></span> **occlusion_darkness** = 0.8

*(This property has no documentation)*

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_occlusion_enabled"></span> **occlusion_enabled** = true

*(This property has no documentation)*

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_shadow_occluder_negative_x"></span> **shadow_occluder_negative_x** = false

When enabled, generates a quad covering the negative X side of the chunk if it is fully covered by opaque voxels, in order to force directional lights to project a shadow.

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_shadow_occluder_negative_y"></span> **shadow_occluder_negative_y** = false

When enabled, generates a quad covering the negative Y side of the chunk if it is fully covered by opaque voxels, in order to force directional lights to project a shadow.

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_shadow_occluder_negative_z"></span> **shadow_occluder_negative_z** = false

When enabled, generates a quad covering the negative Z side of the chunk if it is fully covered by opaque voxels, in order to force directional lights to project a shadow.

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_shadow_occluder_positive_x"></span> **shadow_occluder_positive_x** = false

When enabled, generates a quad covering the positive X side of the chunk if it is fully covered by opaque voxels, in order to force directional lights to project a shadow.

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_shadow_occluder_positive_y"></span> **shadow_occluder_positive_y** = false

When enabled, generates a quad covering the positive Y side of the chunk if it is fully covered by opaque voxels, in order to force directional lights to project a shadow.

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_shadow_occluder_positive_z"></span> **shadow_occluder_positive_z** = false

When enabled, generates a quad covering the positive Z side of the chunk if it is fully covered by opaque voxels, in order to force directional lights to project a shadow.

## Method Descriptions

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_get_shadow_occluder_side"></span> **get_shadow_occluder_side**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) side ) 

*(This method has no documentation)*

### [void](#)<span id="i_set_shadow_occluder_side"></span> **set_shadow_occluder_side**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) side, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled ) 

*(This method has no documentation)*

_Generated on Aug 27, 2024_
