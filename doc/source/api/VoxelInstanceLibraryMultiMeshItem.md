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
`PackedScene`  | [scene](#i_scene)                          |         
<p></p>

## Methods: 


Return                                                                  | Signature                                                                                                                                                                                     
----------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)  | [get_mesh](#i_get_mesh) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mesh_lod_index ) const                                                                         
[void](#)                                                               | [set_mesh](#i_set_mesh) ( [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html) mesh, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mesh_lod_index )  
[void](#)                                                               | [setup_from_template](#i_setup_from_template) ( [Node](https://docs.godotengine.org/en/stable/classes/class_node.html) node )                                                                 
<p></p>

## Constants: 

- **MAX_MESH_LODS** = **4**

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


- [PackedScene](https://docs.godotengine.org/en/stable/classes/class_packedscene.html)<span id="i_scene"></span> **scene**


## Method Descriptions

- [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_get_mesh"></span> **get_mesh**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mesh_lod_index ) 


- [void](#)<span id="i_set_mesh"></span> **set_mesh**( [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html) mesh, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mesh_lod_index ) 


- [void](#)<span id="i_setup_from_template"></span> **setup_from_template**( [Node](https://docs.godotengine.org/en/stable/classes/class_node.html) node ) 


_Generated on Mar 26, 2023_
