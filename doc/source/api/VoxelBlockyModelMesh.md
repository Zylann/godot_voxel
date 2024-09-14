# VoxelBlockyModelMesh

Inherits: [VoxelBlockyModel](VoxelBlockyModel.md)

Generates a model based on a custom mesh.

## Description: 

[VoxelMesherBlocky](VoxelMesherBlocky.md) does not require models to be cubes. Ultimately, model visuals are all meshes. This is the most versatile option to make a model. The workflow is to make these models in a 3D editor such as Blender, making sure they are confined in a box going from (0,0) to (1,1). Textures are assigned with classic UV-mapping.

## Properties: 


Type                                                                      | Name                                                       | Default 
------------------------------------------------------------------------- | ---------------------------------------------------------- | --------
[Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)    | [mesh](#i_mesh)                                            |         
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)      | [mesh_ortho_rotation_index](#i_mesh_ortho_rotation_index)  | 0       
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [side_vertex_tolerance](#i_side_vertex_tolerance)          | 0.001   
<p></p>

## Property Descriptions

### [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_mesh"></span> **mesh**

Mesh that will be used for this model. Must not be empty (if you want an empty model, use [VoxelBlockyModelEmpty](VoxelBlockyModelEmpty.md)). If it has more than 2 surfaces (2 materials) the next surfaces will be ignored. Surfaces must be made of triangles. Must be indexed (if you generate meshes with [SurfaceTool](https://docs.godotengine.org/en/stable/classes/class_surfacetool.html), use [SurfaceTool.index](https://docs.godotengine.org/en/stable/classes/class_surfacetool.html#class-surfacetool-method-index)). Should ideally contain geometry within a 0..1 area. Only triangles located on the sides of that cubic area will be considered for neighbor side culling (see also [VoxelBlockyModelMesh.side_vertex_tolerance](VoxelBlockyModelMesh.md#i_side_vertex_tolerance)).

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_mesh_ortho_rotation_index"></span> **mesh_ortho_rotation_index** = 0

Orthogonal rotation applied to the mesh when baking. Values are taken from the same convention as [GridMap](https://docs.godotengine.org/en/stable/classes/class_gridmap.html) tiles.

([GridMap](https://docs.godotengine.org/en/stable/classes/class_gridmap.html) provides a conversion method from [Basis](https://docs.godotengine.org/en/stable/classes/class_basis.html), unfortunately it is not a static method so it requires a [GridMap](https://docs.godotengine.org/en/stable/classes/class_gridmap.html) instance to exist. A helper method could be added in the future if requested)

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_side_vertex_tolerance"></span> **side_vertex_tolerance** = 0.001

Margin below which triangles located near one of the 6 sides of the voxel will be considered on that side. Sides get culled or not by comparing between triangles of neighbor sides.

_Generated on Aug 27, 2024_
