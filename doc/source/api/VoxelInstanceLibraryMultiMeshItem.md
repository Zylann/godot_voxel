# VoxelInstanceLibraryMultiMeshItem

Inherits: [VoxelInstanceLibraryItem](VoxelInstanceLibraryItem.md)



## Properties: 


Type           | Name                                       | Default 
-------------- | ------------------------------------------ | --------
`int`          | [cast_shadow](#i_cast_shadow)              | 1       
`int`          | [collision_layer](#i_collision_layer)      | 1       
`int`          | [collision_mask](#i_collision_mask)        | 1       
`Array`        | [collision_shapes](#i_collision_shapes)    | []      
`Material`     | [material_override](#i_material_override)  |         
`Mesh`         | [mesh](#i_mesh)                            |         
`Mesh`         | [mesh_lod1](#i_mesh_lod1)                  |         
`Mesh`         | [mesh_lod2](#i_mesh_lod2)                  |         
`Mesh`         | [mesh_lod3](#i_mesh_lod3)                  |         
`int`          | [render_layer](#i_render_layer)            | 1       
`PackedScene`  | [scene](#i_scene)                          |         
<p></p>

## Methods: 


Return                                                                                  | Signature                                                                                                                                                                                     
--------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[StringName[]](https://docs.godotengine.org/en/stable/classes/class_stringname[].html)  | [get_collider_group_names](#i_get_collider_group_names) ( ) const                                                                                                                             
[Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)                  | [get_mesh](#i_get_mesh) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mesh_lod_index ) const                                                                         
[void](#)                                                                               | [set_collider_group_names](#i_set_collider_group_names) ( [StringName[]](https://docs.godotengine.org/en/stable/classes/class_stringname[].html) names )                                      
[void](#)                                                                               | [set_mesh](#i_set_mesh) ( [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html) mesh, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mesh_lod_index )  
[void](#)                                                                               | [setup_from_template](#i_setup_from_template) ( [Node](https://docs.godotengine.org/en/stable/classes/class_node.html) node )                                                                 
<p></p>

## Constants: 

- <span id="i_MAX_MESH_LODS"></span>**MAX_MESH_LODS** = **4**

## Property Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_cast_shadow"></span> **cast_shadow** = 1


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_collision_layer"></span> **collision_layer** = 1


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_collision_mask"></span> **collision_mask** = 1


- [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_collision_shapes"></span> **collision_shapes** = []


- [Material](https://docs.godotengine.org/en/stable/classes/class_material.html)<span id="i_material_override"></span> **material_override**


- [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_mesh"></span> **mesh**


- [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_mesh_lod1"></span> **mesh_lod1**


- [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_mesh_lod2"></span> **mesh_lod2**


- [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_mesh_lod3"></span> **mesh_lod3**


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_render_layer"></span> **render_layer** = 1


- [PackedScene](https://docs.godotengine.org/en/stable/classes/class_packedscene.html)<span id="i_scene"></span> **scene**


## Method Descriptions

- [StringName[]](https://docs.godotengine.org/en/stable/classes/class_stringname[].html)<span id="i_get_collider_group_names"></span> **get_collider_group_names**( ) 

Gets the list of group names that are added to collider nodes.

- [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_get_mesh"></span> **get_mesh**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mesh_lod_index ) 


- [void](#)<span id="i_set_collider_group_names"></span> **set_collider_group_names**( [StringName[]](https://docs.godotengine.org/en/stable/classes/class_stringname[].html) names ) 

Sets the list of group names that will be added to collider nodes generated for each instance.

- [void](#)<span id="i_set_mesh"></span> **set_mesh**( [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html) mesh, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mesh_lod_index ) 


- [void](#)<span id="i_setup_from_template"></span> **setup_from_template**( [Node](https://docs.godotengine.org/en/stable/classes/class_node.html) node ) 


_Generated on Nov 11, 2023_
