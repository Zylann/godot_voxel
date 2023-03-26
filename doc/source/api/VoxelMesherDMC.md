# VoxelMesherDMC

Inherits: [VoxelMesher](VoxelMesher.md)


Implements isosurface generation (smooth voxels) using [Dual Marching Cubes](https://www.volume-gfx.com/volume-rendering/dual-marching-cubes/).

## Properties: 


Type   | Name                                   | Default 
------ | -------------------------------------- | --------
`int`  | [geometric_error](#i_geometric_error)  | 0       
`int`  | [mesh_mode](#i_mesh_mode)              | 0       
`int`  | [seam_mode](#i_seam_mode)              | 0       
`int`  | [simplify_mode](#i_simplify_mode)      | 0       
<p></p>

## Methods: 


Return                                                                              | Signature                                                                                                                         
----------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)            | [get_geometric_error](#i_get_geometric_error) ( ) const                                                                           
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)  | [get_statistics](#i_get_statistics) ( ) const                                                                                     
[void](#)                                                                           | [set_geometric_error](#i_set_geometric_error) ( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) error )  
<p></p>

## Enumerations: 

enum **MeshMode**: 

- **MESH_NORMAL** = **0**
- **MESH_WIREFRAME** = **1**
- **MESH_DEBUG_OCTREE** = **2**
- **MESH_DEBUG_DUAL_GRID** = **3**

enum **SimplifyMode**: 

- **SIMPLIFY_OCTREE_BOTTOM_UP** = **0**
- **SIMPLIFY_OCTREE_TOP_DOWN** = **1**
- **SIMPLIFY_NONE** = **2**

enum **SeamMode**: 

- **SEAM_NONE** = **0**
- **SEAM_MARCHING_SQUARE_SKIRTS** = **1**


## Property Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_geometric_error"></span> **geometric_error** = 0


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_mesh_mode"></span> **mesh_mode** = 0


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_seam_mode"></span> **seam_mode** = 0


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_simplify_mode"></span> **simplify_mode** = 0


## Method Descriptions

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_get_geometric_error"></span> **get_geometric_error**( ) 


- [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)<span id="i_get_statistics"></span> **get_statistics**( ) 


- [void](#)<span id="i_set_geometric_error"></span> **set_geometric_error**( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) error ) 


_Generated on Mar 26, 2023_
