# VoxelInstanceLibraryMultiMeshItem

Inherits: [VoxelInstanceLibraryItem](VoxelInstanceLibraryItem.md)

Instancer model using [MultiMesh](https://docs.godotengine.org/en/stable/classes/class_multimesh.html) to render.

## Description: 

This model is suited for rendering very large amounts of simple instances, such as grass and rocks.

## Properties: 


Type                  | Name                                                       | Default                               
--------------------- | ---------------------------------------------------------- | --------------------------------------
`PackedFloat32Array`  | [_mesh_lod_distance_ratios](#i__mesh_lod_distance_ratios)  | PackedFloat32Array(0.2, 0.35, 0.6, 1) 
`int`                 | [cast_shadow](#i_cast_shadow)                              | 1                                     
`int`                 | [collision_layer](#i_collision_layer)                      | 1                                     
`int`                 | [collision_mask](#i_collision_mask)                        | 1                                     
`Array`               | [collision_shapes](#i_collision_shapes)                    | []                                    
`int`                 | [gi_mode](#i_gi_mode)                                      | 1                                     
`bool`                | [hide_beyond_max_lod](#i_hide_beyond_max_lod)              | false                                 
`Material`            | [material_override](#i_material_override)                  |                                       
`Mesh`                | [mesh](#i_mesh)                                            |                                       
`float`               | [mesh_lod0_distance_ratio](#i_mesh_lod0_distance_ratio)    | 0.2                                   
`Mesh`                | [mesh_lod1](#i_mesh_lod1)                                  |                                       
`float`               | [mesh_lod1_distance_ratio](#i_mesh_lod1_distance_ratio)    | 0.35                                  
`Mesh`                | [mesh_lod2](#i_mesh_lod2)                                  |                                       
`float`               | [mesh_lod2_distance_ratio](#i_mesh_lod2_distance_ratio)    | 0.6                                   
`Mesh`                | [mesh_lod3](#i_mesh_lod3)                                  |                                       
`float`               | [mesh_lod3_distance_ratio](#i_mesh_lod3_distance_ratio)    | 1.0                                   
`int`                 | [render_layer](#i_render_layer)                            | 1                                     
`PackedScene`         | [scene](#i_scene)                                          |                                       
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

- [PackedFloat32Array](https://docs.godotengine.org/en/stable/classes/class_packedfloat32array.html)<span id="i__mesh_lod_distance_ratios"></span> **_mesh_lod_distance_ratios** = PackedFloat32Array(0.2, 0.35, 0.6, 1)


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_cast_shadow"></span> **cast_shadow** = 1


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_collision_layer"></span> **collision_layer** = 1


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_collision_mask"></span> **collision_mask** = 1


- [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_collision_shapes"></span> **collision_shapes** = []


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_gi_mode"></span> **gi_mode** = 1


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_hide_beyond_max_lod"></span> **hide_beyond_max_lod** = false


- [Material](https://docs.godotengine.org/en/stable/classes/class_material.html)<span id="i_material_override"></span> **material_override**


- [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_mesh"></span> **mesh**


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_mesh_lod0_distance_ratio"></span> **mesh_lod0_distance_ratio** = 0.2


- [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_mesh_lod1"></span> **mesh_lod1**


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_mesh_lod1_distance_ratio"></span> **mesh_lod1_distance_ratio** = 0.35


- [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_mesh_lod2"></span> **mesh_lod2**


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_mesh_lod2_distance_ratio"></span> **mesh_lod2_distance_ratio** = 0.6


- [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_mesh_lod3"></span> **mesh_lod3**


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_mesh_lod3_distance_ratio"></span> **mesh_lod3_distance_ratio** = 1.0


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


_Generated on Dec 31, 2023_
