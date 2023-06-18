# VoxelBlockyLibrary

Inherits: [VoxelBlockyLibraryBase](VoxelBlockyLibraryBase.md)


Contains a list of models that can be used by [VoxelMesherBlocky](VoxelMesherBlocky.md).

## Description: 

Provides a list of models that can be used by [VoxelMesherBlocky](VoxelMesherBlocky.md). Each model corresponds to an ID in voxel data, and is generally defined from a mesh. Some extra properties can also be defined, such as how sides get culled by neighbor voxels, or how it is treated by some functionality of the voxel engine.

If you create this library from code, it needs to be baked at the end using the method bake function.

The first model (at index 0) is conventionally used for "air" or "empty".

## Properties: 


Type                  | Name                 | Default 
--------------------- | -------------------- | --------
`VoxelBlockyModel[]`  | [models](#i_models)  | []      
<p></p>

## Methods: 


Return                                                                | Signature                                                                                                                                                             
--------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [add_model](#i_add_model) ( [VoxelBlockyModel](VoxelBlockyModel.md) _unnamed_arg0 )                                                                                   
[VoxelBlockyModel](VoxelBlockyModel.md)                               | [get_model](#i_get_model) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) index ) const                                                        
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [get_model_index_from_resource_name](#i_get_model_index_from_resource_name) ( [String](https://docs.godotengine.org/en/stable/classes/class_string.html) name ) const 
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [get_voxel_index_from_name](#i_get_voxel_index_from_name) ( [String](https://docs.godotengine.org/en/stable/classes/class_string.html) name ) const                   
<p></p>

## Property Descriptions

- [VoxelBlockyModel[]](https://docs.godotengine.org/en/stable/classes/class_voxelblockymodel[].html)<span id="i_models"></span> **models** = []

Array of all the models. The index of each model corresponds to the value representing them in voxel data's TYPE channel.

## Method Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_add_model"></span> **add_model**( [VoxelBlockyModel](VoxelBlockyModel.md) _unnamed_arg0 ) 

Adds a model to the library. Returns its index, which will be the value of voxels representing it.

- [VoxelBlockyModel](VoxelBlockyModel.md)<span id="i_get_model"></span> **get_model**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) index ) 

Gets a model from its index.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_model_index_from_resource_name"></span> **get_model_index_from_resource_name**( [String](https://docs.godotengine.org/en/stable/classes/class_string.html) name ) 

Finds the index of the first model having the specified resource name. If not found, returns `null`.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_voxel_index_from_name"></span> **get_voxel_index_from_name**( [String](https://docs.godotengine.org/en/stable/classes/class_string.html) name ) 

Finds the index of the first model having the specified resource name. If not found, returns `null`.

_Generated on Jun 18, 2023_
