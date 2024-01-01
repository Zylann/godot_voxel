# VoxelBlockyModelMesh

Inherits: [VoxelBlockyModel](VoxelBlockyModel.md)

Generates a model based on a custom mesh.

## Description: 

[VoxelMesherBlocky](VoxelMesherBlocky.md) does not require models to be cubes. Ultimately, model visuals are all meshes. This is the most versatile option to make a model. The workflow is to make these models in a 3D editor such as Blender, making sure they are confined in a box going from (0,0) to (1,1). Textures are assigned with classic UV-mapping.

## Properties: 


Type    | Name                                                       | Default 
------- | ---------------------------------------------------------- | --------
`Mesh`  | [mesh](#i_mesh)                                            |         
`int`   | [mesh_ortho_rotation_index](#i_mesh_ortho_rotation_index)  | 0       
<p></p>

## Property Descriptions

- [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_mesh"></span> **mesh**


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_mesh_ortho_rotation_index"></span> **mesh_ortho_rotation_index** = 0

Orthogonal rotation applied to the mesh when baking. Values are taken from the same convention as [GridMap](https://docs.godotengine.org/en/stable/classes/class_gridmap.html) tiles.

([GridMap](https://docs.godotengine.org/en/stable/classes/class_gridmap.html) provides a conversion method from [Basis](https://docs.godotengine.org/en/stable/classes/class_basis.html), unfortunately it is not a static method so it requires a [GridMap](https://docs.godotengine.org/en/stable/classes/class_gridmap.html) instance to exist. A helper method could be added in the future if requested)

_Generated on Dec 31, 2023_
