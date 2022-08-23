# VoxelBlockyLibrary

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)




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


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_bake_tangents"></span> **bake_tangents** = true

Enable this option if you need normal mapping on your voxels. If you don't need it, disabling can reduce memory usage and give a small speed boost.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_voxel_count"></span> **voxel_count** = 0


## Method Descriptions

- [void](#)<span id="i_bake"></span> **bake**( ) 


- [VoxelBlockyModel](VoxelBlockyModel.md)<span id="i_create_voxel"></span> **create_voxel**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) id, [String](https://docs.godotengine.org/en/stable/classes/class_string.html) name ) 


- [Material[]](https://docs.godotengine.org/en/stable/classes/class_material[].html)<span id="i_get_materials"></span> **get_materials**( ) 


- [VoxelBlockyModel](VoxelBlockyModel.md)<span id="i_get_voxel"></span> **get_voxel**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) id ) 


- [VoxelBlockyModel](VoxelBlockyModel.md)<span id="i_get_voxel_by_name"></span> **get_voxel_by_name**( [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html) name ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_voxel_index_from_name"></span> **get_voxel_index_from_name**( [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html) name ) 


_Generated on Aug 20, 2022_
