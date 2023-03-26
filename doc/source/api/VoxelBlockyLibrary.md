# VoxelBlockyLibrary

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)


Contains a list of models that can be used by [VoxelMesherBlocky](VoxelMesherBlocky.md).

## Description: 

Provides a list of models that can be used by [VoxelMesherBlocky](VoxelMesherBlocky.md). Each model corresponds to an ID in voxel data, and is generally defined from a mesh. Some extra properties can also be defined, such as how sides get culled by neighbor voxels, or how it is treated by some functionality of the voxel engine.

If you create this library from code, it needs to be baked at the end using the method bake function.

The first model (at index 0) is conventionally used for "air" or "empty".

## Properties: 


Type    | Name                               | Default 
------- | ---------------------------------- | --------
`int`   | [atlas_size](#i_atlas_size)        | 16      
`bool`  | [bake_tangents](#i_bake_tangents)  | true    
`int`   | [voxel_count](#i_voxel_count)      | 0       
<p></p>

## Methods: 


Return                                                                              | Signature                                                                                                                                                                                     
----------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                                           | [bake](#i_bake) ( )                                                                                                                                                                           
[VoxelBlockyModel](VoxelBlockyModel.md)                                             | [create_voxel](#i_create_voxel) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) id, [String](https://docs.godotengine.org/en/stable/classes/class_string.html) name )  
[Material[]](https://docs.godotengine.org/en/stable/classes/class_material[].html)  | [get_materials](#i_get_materials) ( ) const                                                                                                                                                   
[VoxelBlockyModel](VoxelBlockyModel.md)                                             | [get_voxel](#i_get_voxel) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) id )                                                                                         
[VoxelBlockyModel](VoxelBlockyModel.md)                                             | [get_voxel_by_name](#i_get_voxel_by_name) ( [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html) name )                                                         
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [get_voxel_index_from_name](#i_get_voxel_index_from_name) ( [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html) name ) const                                   
<p></p>

## Constants: 

- **MAX_VOXEL_TYPES** = **65536**

## Property Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_atlas_size"></span> **atlas_size** = 16

Sets a reference size of texture atlas, in tiles. The atlas is assumed to be square (same number of tiles horizontally and vertically). This is only used on models which have a geometry mode set to `GEOMETRY_CUBE`.

This property is old and might be removed or changed in the future, as it only works in specific setups.

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_bake_tangents"></span> **bake_tangents** = true

Enable this option if you need normal mapping on your voxels. If you don't need it, disabling can reduce memory usage and give a small speed boost.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_voxel_count"></span> **voxel_count** = 0

How many models the library can contain. You may set it first before adding models from a script.

## Method Descriptions

- [void](#)<span id="i_bake"></span> **bake**( ) 

Bakes the library. The data of models is optimized in order to combine them more efficiently when generating voxel meshes.

- [VoxelBlockyModel](VoxelBlockyModel.md)<span id="i_create_voxel"></span> **create_voxel**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) id, [String](https://docs.godotengine.org/en/stable/classes/class_string.html) name ) 

Creates a new model attached to this library.

`id`: ID of the model. It must be comprised between 0 and member voxel_count excluded. This ID will be used in voxel data.

`name`: Name of the model. It is convenient to give one for better organization.

- [Material[]](https://docs.godotengine.org/en/stable/classes/class_material[].html)<span id="i_get_materials"></span> **get_materials**( ) 

Gets a list of all distinct materials found in all models of the library.

Note, if at least one non-empty model has no material, there will be one `null` entry in this list to represent "The default material".

- [VoxelBlockyModel](VoxelBlockyModel.md)<span id="i_get_voxel"></span> **get_voxel**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) id ) 

Gets a model from its ID.

- [VoxelBlockyModel](VoxelBlockyModel.md)<span id="i_get_voxel_by_name"></span> **get_voxel_by_name**( [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html) name ) 

Finds the first model having the specified name. If not found, returns `null`.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_voxel_index_from_name"></span> **get_voxel_index_from_name**( [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html) name ) 

Finds the ID of the first model having the specified name. If not found, returns `null`.

_Generated on Mar 26, 2023_
