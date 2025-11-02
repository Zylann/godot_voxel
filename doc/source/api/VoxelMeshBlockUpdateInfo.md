# VoxelMeshBlockUpdateInfo

Inherits: [Object](https://docs.godotengine.org/en/stable/classes/class_object.html)

Information sent by a terrain when one of its meshes is created or updated.

## Description: 

Information sent by a terrain when one of its meshes is created or updated. See [VoxelTerrain._on_mesh_block_udpate](VoxelTerrain.md#i__on_mesh_block_udpate).

Instances of this class must not be stored, as they will become invalid after the call they come from.

## Methods: 


Return                                                                          | Signature                                             
------------------------------------------------------------------------------- | ------------------------------------------------------
[Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)  | [get_grid_position](#i_get_grid_position) ( ) const   
[Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)    | [get_local_position](#i_get_local_position) ( ) const 
[Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)          | [get_mesh](#i_get_mesh) ( ) const                     
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)          | [is_first](#i_is_first) ( ) const                     
<p></p>

## Method Descriptions

### [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)<span id="i_get_grid_position"></span> **get_grid_position**( ) 

Position of the mesh in block coordinates.

### [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)<span id="i_get_local_position"></span> **get_local_position**( ) 

Position of the mesh in local coordinates (to the terrain).

### [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_get_mesh"></span> **get_mesh**( ) 

Gets the mesh for this block. It can be null if no surface is present (there is no surface if the area contains only air or doesn't contain air).

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_is_first"></span> **is_first**( ) 

Returns `false` if this is the first time this block was meshed within a viewer area. Testing this is equivalent to using

_Generated on Nov 02, 2025_
