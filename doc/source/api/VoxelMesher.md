# VoxelMesher

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)


Base class for all meshing algorithms.

## Description: 

In order to be rendered by Godot, voxels can be transformed into a mesh. There are various ways to do this, that's why this class is only a base for other, specialized ones. Voxel nodes automatically make use of meshers, but you can also produce meshes manually. For this, you may use one of the derived classes. Meshers can be re-used, which often yields better performance by reducing memory allocations.

## Methods: 


Return                                                                  | Signature                                                                                                                                                                                                                                                                        
----------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)  | [build_mesh](#i_build_mesh) ( [VoxelBuffer](VoxelBuffer.md) voxel_buffer, [Material[]](https://docs.godotengine.org/en/stable/classes/class_material[].html) materials, [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html) additional_data={} )  
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)    | [get_maximum_padding](#i_get_maximum_padding) ( ) const                                                                                                                                                                                                                          
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)    | [get_minimum_padding](#i_get_minimum_padding) ( ) const                                                                                                                                                                                                                          
<p></p>

## Method Descriptions

- [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_build_mesh"></span> **build_mesh**( [VoxelBuffer](VoxelBuffer.md) voxel_buffer, [Material[]](https://docs.godotengine.org/en/stable/classes/class_material[].html) materials, [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html) additional_data={} ) 

Builds a mesh from the provided voxels. Materials will be attached to each surface based on the provided array. The way materials are used can depend on the type of mesher.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_maximum_padding"></span> **get_maximum_padding**( ) 

Gets by how much voxels must be padded before their lower corner in order for the mesher to work.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_minimum_padding"></span> **get_minimum_padding**( ) 

Gets by how much voxels must be padded after their upper corner in order for the mesher to work.

_Generated on Mar 26, 2023_
