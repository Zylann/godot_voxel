# VoxelBlockyModel

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)


Model stored in [VoxelBlockyLibrary](VoxelBlockyLibrary.md) and used by [VoxelMesherBlocky](VoxelMesherBlocky.md).

## Description: 

Represents a model to be used for voxels of a specific TYPE value. Such models must be contained within a [VoxelBlockyLibrary](VoxelBlockyLibrary.md) to be used with [VoxelTerrain](VoxelTerrain.md) or directly with a [VoxelMesherBlocky](VoxelMesherBlocky.md).

Some other various properties also exist to make it easier to implement games based on this technique (such as Minecraft).

## Properties: 


Type          | Name                                         | Default           
------------- | -------------------------------------------- | ------------------
`AABB[]`      | [collision_aabbs](#i_collision_aabbs)        | []                
`int`         | [collision_mask](#i_collision_mask)          | 1                 
`Color`       | [color](#i_color)                            | Color(1, 1, 1, 1) 
`Mesh`        | [custom_mesh](#i_custom_mesh)                |                   
`int`         | [geometry_type](#i_geometry_type)            | 0                 
`bool`        | [random_tickable](#i_random_tickable)        | false             
`int`         | [transparency_index](#i_transparency_index)  | 0                 
`bool`        | [transparent](#i_transparent)                | false             
`StringName`  | [voxel_name](#i_voxel_name)                  | &""               
<p></p>

## Methods: 


Return                                                                          | Signature                                                                                                                                                                                                                           
------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)            | [get_id](#i_get_id) ( ) const                                                                                                                                                                                                       
[Material](https://docs.godotengine.org/en/stable/classes/class_material.html)  | [get_material_override](#i_get_material_override) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) index ) const                                                                                              
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)          | [is_empty](#i_is_empty) ( ) const                                                                                                                                                                                                   
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)          | [is_mesh_collision_enabled](#i_is_mesh_collision_enabled) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) surface_index ) const                                                                              
[void](#)                                                                       | [set_id](#i_set_id) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) id )                                                                                                                                     
[void](#)                                                                       | [set_material_override](#i_set_material_override) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) index, [Material](https://docs.godotengine.org/en/stable/classes/class_material.html) material )           
[void](#)                                                                       | [set_mesh_collision_enabled](#i_set_mesh_collision_enabled) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) surface_index, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled )  
<p></p>

## Enumerations: 

enum **GeometryType**: 

- **GEOMETRY_NONE** = **0** --- Don't produce any geometry. The voxel will be invisible.
- **GEOMETRY_CUBE** = **1** --- Use the shape of a generated cube. It is useful for testing and quick configuration. In this mode, tile positions for sides of the cube are made available in the editor. They refer to coordinates of tiles in a texture atlas. You may use an atlas in order to use them. The size of the atlas may be set in [member VoxelBlockyLibrary.atlas_size].
- **GEOMETRY_CUSTOM_MESH** = **2** --- Use the mesh specified in the [member mesh] property. This is the most versatile way to create shapes.
- **GEOMETRY_MAX** = **3** --- How many geometry modes there are.

enum **Side**: 

- **SIDE_NEGATIVE_X** = **1**
- **SIDE_POSITIVE_X** = **0**
- **SIDE_NEGATIVE_Y** = **2**
- **SIDE_POSITIVE_Y** = **3**
- **SIDE_NEGATIVE_Z** = **4**
- **SIDE_POSITIVE_Z** = **5**
- **SIDE_COUNT** = **6**


## Property Descriptions

- [AABB[]](https://docs.godotengine.org/en/stable/classes/class_aabb[].html)<span id="i_collision_aabbs"></span> **collision_aabbs** = []

List of bounding boxes relative to the model. They are used for box-based collision, using [VoxelBoxMover](VoxelBoxMover.md). They are not used with mesh-based collision.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_collision_mask"></span> **collision_mask** = 1

Collision mask used for box-based collision [VoxelBoxMover](VoxelBoxMover.md) and voxel raycasts ([VoxelToolTerrain](VoxelToolTerrain.md)). It is not used for mesh-based collisions.

- [Color](https://docs.godotengine.org/en/stable/classes/class_color.html)<span id="i_color"></span> **color** = Color(1, 1, 1, 1)

Color of the model. It will be used to modulate its color when built into a voxel mesh.

- [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_custom_mesh"></span> **custom_mesh**

Specifies the mesh of the model. Ultimately, all models use a mesh.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_geometry_type"></span> **geometry_type** = 0

Tells which geometry type to use. Most of the time you may use constant GEOMETRY_CUSTOM_MESH, but some shortcuts can be used for cubes or empty models.

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_random_tickable"></span> **random_tickable** = false

If enabled, voxels having this ID in the TYPE channel will be used by method VoxelToolTerrain.run_blocky_random_tick.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_transparency_index"></span> **transparency_index** = 0

Determines how transparency is handled when the sides of the model are culled by neighbor voxels.

Equal indices culls the face, different indexes doesn't.

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_transparent"></span> **transparent** = false

Tells if the model is transparent in the context of sides being culled by neighbor voxels.

This is a legacy property, member transparency_index may be used instead.

- [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html)<span id="i_voxel_name"></span> **voxel_name** = &""

Name that can be used for convenience, when looking up a specific [VoxelBlockyModel](VoxelBlockyModel.md) from [VoxelBlockyLibrary](VoxelBlockyLibrary.md).

## Method Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_id"></span> **get_id**( ) 

Gets the ID of the model. This ID is used in voxel data.

- [Material](https://docs.godotengine.org/en/stable/classes/class_material.html)<span id="i_get_material_override"></span> **get_material_override**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) index ) 

Gets the material override for a specific surface of the model.

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_is_empty"></span> **is_empty**( ) 

Tells if the model contains any geometry.

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_is_mesh_collision_enabled"></span> **is_mesh_collision_enabled**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) surface_index ) 

Tells if a specific surface produces mesh-based collisions.

- [void](#)<span id="i_set_id"></span> **set_id**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) id ) 

Sets the ID of the model.

This method is deprecated. Changing the ID of a model after it's been added to a library is not supported.

- [void](#)<span id="i_set_material_override"></span> **set_material_override**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) index, [Material](https://docs.godotengine.org/en/stable/classes/class_material.html) material ) 

Sets a material override for a specific surface of the model. It allows to use the same mesh on multiple models, but using different materials on each.

- [void](#)<span id="i_set_mesh_collision_enabled"></span> **set_mesh_collision_enabled**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) surface_index, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled ) 

Enables or disables mesh-based collision on a specific surface. It allows a model to have solid parts and others where players can pass through.

_Generated on Mar 26, 2023_
