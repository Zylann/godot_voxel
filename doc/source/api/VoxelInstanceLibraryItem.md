# VoxelInstanceLibraryItem

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)


Settings for a model that can be used by [VoxelInstancer](VoxelInstancer.md)

## Properties: 


Type                      | Name                                       | Default 
------------------------- | ------------------------------------------ | --------
`int`                     | [cast_shadow](#i_cast_shadow)              | 1       
`int`                     | [collision_layer](#i_collision_layer)      | 1       
`int`                     | [collision_mask](#i_collision_mask)        | 1       
`VoxelInstanceGenerator`  | [generator](#i_generator)                  |         
`int`                     | [lod_index](#i_lod_index)                  | 0       
`Material`                | [material_override](#i_material_override)  |         
`Mesh`                    | [mesh](#i_mesh)                            |         
`Mesh`                    | [mesh_lod1](#i_mesh_lod1)                  |         
`Mesh`                    | [mesh_lod2](#i_mesh_lod2)                  |         
`Mesh`                    | [mesh_lod3](#i_mesh_lod3)                  |         
`String`                  | [name](#i_name)                            | ""      
`bool`                    | [persistent](#i_persistent)                | false   
<p></p>

## Methods: 


Return                                                                    | Signature                                                                                                                                                                                     
------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[Array](https://docs.godotengine.org/en/stable/classes/class_array.html)  | [get_collision_shapes](#i_get_collision_shapes) ( ) const                                                                                                                                     
[Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)    | [get_mesh](#i_get_mesh) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mesh_lod_index ) const                                                                         
[void](#)                                                                 | [set_collision_shapes](#i_set_collision_shapes) ( [Array](https://docs.godotengine.org/en/stable/classes/class_array.html) shape_infos )                                                      
[void](#)                                                                 | [set_mesh](#i_set_mesh) ( [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html) mesh, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mesh_lod_index )  
[void](#)                                                                 | [setup_from_template](#i_setup_from_template) ( [Node](https://docs.godotengine.org/en/stable/classes/class_node.html) node )                                                                 
<p></p>

## Constants: 

- **MAX_MESH_LODS** = **4**

## Property Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_cast_shadow"></span> **cast_shadow** = 1


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_collision_layer"></span> **collision_layer** = 1


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_collision_mask"></span> **collision_mask** = 1


- [VoxelInstanceGenerator](VoxelInstanceGenerator.md)<span id="i_generator"></span> **generator**


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_lod_index"></span> **lod_index** = 0


- [Material](https://docs.godotengine.org/en/stable/classes/class_material.html)<span id="i_material_override"></span> **material_override**


- [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_mesh"></span> **mesh**


- [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_mesh_lod1"></span> **mesh_lod1**


- [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_mesh_lod2"></span> **mesh_lod2**


- [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_mesh_lod3"></span> **mesh_lod3**


- [String](https://docs.godotengine.org/en/stable/classes/class_string.html)<span id="i_name"></span> **name** = ""


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_persistent"></span> **persistent** = false


## Method Descriptions

- [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_get_collision_shapes"></span> **get_collision_shapes**( ) 


- [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_get_mesh"></span> **get_mesh**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mesh_lod_index ) 


- [void](#)<span id="i_set_collision_shapes"></span> **set_collision_shapes**( [Array](https://docs.godotengine.org/en/stable/classes/class_array.html) shape_infos ) 


- [void](#)<span id="i_set_mesh"></span> **set_mesh**( [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html) mesh, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mesh_lod_index ) 


- [void](#)<span id="i_setup_from_template"></span> **setup_from_template**( [Node](https://docs.godotengine.org/en/stable/classes/class_node.html) node ) 


_Generated on Apr 10, 2021_
