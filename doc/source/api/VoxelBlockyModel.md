# VoxelBlockyModel

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)

Inherited by: [VoxelBlockyModelCube](VoxelBlockyModelCube.md), [VoxelBlockyModelEmpty](VoxelBlockyModelEmpty.md), [VoxelBlockyModelFluid](VoxelBlockyModelFluid.md), [VoxelBlockyModelMesh](VoxelBlockyModelMesh.md)

Model stored in [VoxelBlockyLibrary](VoxelBlockyLibrary.md) and used by [VoxelMesherBlocky](VoxelMesherBlocky.md).

## Description: 

Represents a model to be used for voxels of a specific TYPE value. Such models must be contained within a [VoxelBlockyLibrary](VoxelBlockyLibrary.md) to be used with [VoxelTerrain](VoxelTerrain.md) or directly with a [VoxelMesherBlocky](VoxelMesherBlocky.md).

A model can be setup in various ways, see child classes.

## Properties: 


Type                                                                        | Name                                         | Default           
--------------------------------------------------------------------------- | -------------------------------------------- | ------------------
[AABB[]](https://docs.godotengine.org/en/stable/classes/class_aabb[].html)  | [collision_aabbs](#i_collision_aabbs)        | []                
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)        | [collision_mask](#i_collision_mask)          | 1                 
[Color](https://docs.godotengine.org/en/stable/classes/class_color.html)    | [color](#i_color)                            | Color(1, 1, 1, 1) 
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)      | [culls_neighbors](#i_culls_neighbors)        | true              
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)      | [lod_skirts_enabled](#i_lod_skirts_enabled)  | true              
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)      | [random_tickable](#i_random_tickable)        | false             
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)        | [transparency_index](#i_transparency_index)  | 0                 
<p></p>

## Methods: 


Return                                                                          | Signature                                                                                                                                                                                                                           
------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[Material](https://docs.godotengine.org/en/stable/classes/class_material.html)  | [get_material_override](#i_get_material_override) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) index ) const                                                                                              
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)            | [get_mesh_ortho_rotation_index](#i_get_mesh_ortho_rotation_index) ( ) const                                                                                                                                                         
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)          | [is_mesh_collision_enabled](#i_is_mesh_collision_enabled) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) surface_index ) const                                                                              
[void](#)                                                                       | [rotate_90](#i_rotate_90) ( [Axis](https://docs.godotengine.org/en/stable/classes/class_vector3i.html#enum-vector3i-axis) axis, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) clockwise )                  
[void](#)                                                                       | [set_material_override](#i_set_material_override) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) index, [Material](https://docs.godotengine.org/en/stable/classes/class_material.html) material )           
[void](#)                                                                       | [set_mesh_collision_enabled](#i_set_mesh_collision_enabled) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) surface_index, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled )  
[void](#)                                                                       | [set_mesh_ortho_rotation_index](#i_set_mesh_ortho_rotation_index) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) i )                                                                                        
<p></p>

## Enumerations: 

enum **Side**: 

- <span id="i_SIDE_NEGATIVE_X"></span>**SIDE_NEGATIVE_X** = **1**
- <span id="i_SIDE_POSITIVE_X"></span>**SIDE_POSITIVE_X** = **0**
- <span id="i_SIDE_NEGATIVE_Y"></span>**SIDE_NEGATIVE_Y** = **2**
- <span id="i_SIDE_POSITIVE_Y"></span>**SIDE_POSITIVE_Y** = **3**
- <span id="i_SIDE_NEGATIVE_Z"></span>**SIDE_NEGATIVE_Z** = **4**
- <span id="i_SIDE_POSITIVE_Z"></span>**SIDE_POSITIVE_Z** = **5**
- <span id="i_SIDE_COUNT"></span>**SIDE_COUNT** = **6**


## Property Descriptions

### [AABB[]](https://docs.godotengine.org/en/stable/classes/class_aabb[].html)<span id="i_collision_aabbs"></span> **collision_aabbs** = []

List of bounding boxes relative to the model. They are used for box-based collision, using [VoxelBoxMover](VoxelBoxMover.md). They are not used with mesh-based collision.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_collision_mask"></span> **collision_mask** = 1

Collision mask used for box-based collision [VoxelBoxMover](VoxelBoxMover.md) and voxel raycasts ([VoxelToolTerrain](VoxelToolTerrain.md)). It is not used for mesh-based collisions.

### [Color](https://docs.godotengine.org/en/stable/classes/class_color.html)<span id="i_color"></span> **color** = Color(1, 1, 1, 1)

Color of the model. It will be used to modulate its color when built into a voxel mesh.

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_culls_neighbors"></span> **culls_neighbors** = true

If enabled, this voxel culls the faces of its neighbors. Disabling can be useful for denser transparent voxels, such as foliage.

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_lod_skirts_enabled"></span> **lod_skirts_enabled** = true

If enabled and the terrain has LODs, this model will produce skirts when located at the edge of a chunk. This is intented to hide "cracks" between chunks of different LOD.

You may turn this off if the model is transparent, as the skirt would become visible from behind other surfaces.

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_random_tickable"></span> **random_tickable** = false

If enabled, voxels having this ID in the TYPE channel will be used by [VoxelToolTerrain.run_blocky_random_tick](VoxelToolTerrain.md#i_run_blocky_random_tick).

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_transparency_index"></span> **transparency_index** = 0

Determines how transparency is handled when the sides of the model are culled by neighbor voxels.

If the neighbor voxel at a given side has a transparency index lower or equal to the current voxel, the side will be culled.

## Method Descriptions

### [Material](https://docs.godotengine.org/en/stable/classes/class_material.html)<span id="i_get_material_override"></span> **get_material_override**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) index ) 

Gets the material override for a specific surface of the model.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_mesh_ortho_rotation_index"></span> **get_mesh_ortho_rotation_index**( ) 

Gets the 90-degree rotation ID that will be applied to the model when the library is baked.

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_is_mesh_collision_enabled"></span> **is_mesh_collision_enabled**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) surface_index ) 

Tells if a specific surface produces mesh-based collisions.

### [void](#)<span id="i_rotate_90"></span> **rotate_90**( [Axis](https://docs.godotengine.org/en/stable/classes/class_vector3i.html#enum-vector3i-axis) axis, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) clockwise ) 

*(This method has no documentation)*

### [void](#)<span id="i_set_material_override"></span> **set_material_override**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) index, [Material](https://docs.godotengine.org/en/stable/classes/class_material.html) material ) 

Sets a material override for a specific surface of the model. It allows to use the same mesh on multiple models, but using different materials on each.

### [void](#)<span id="i_set_mesh_collision_enabled"></span> **set_mesh_collision_enabled**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) surface_index, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled ) 

Enables or disables mesh-based collision on a specific surface. It allows a model to have solid parts and others where players can pass through.

### [void](#)<span id="i_set_mesh_ortho_rotation_index"></span> **set_mesh_ortho_rotation_index**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) i ) 

Gets the 90-degree rotation ID that will be applied to the model when the library is baked. This is a number representing one of 24 possible 90-degree rotations. You can also use [rotate_90](VoxelBlockyModel.md#i_rotate_90).

_Generated on May 15, 2025_
