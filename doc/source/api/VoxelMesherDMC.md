# VoxelMesherDMC

Inherits: [VoxelMesher](VoxelMesher.md)

!!! warning
    This class is deprecated. Prefer using [VoxelMesherTransvoxel](VoxelMesherTransvoxel.md) instead.

Implements isosurface generation (smooth voxels) using [Dual Marching Cubes](https://www.volume-gfx.com/volume-rendering/dual-marching-cubes/).

## Properties: 


Type                                                                  | Name                                   | Default 
--------------------------------------------------------------------- | -------------------------------------- | --------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [geometric_error](#i_geometric_error)  | 0       
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [mesh_mode](#i_mesh_mode)              | 0       
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [seam_mode](#i_seam_mode)              | 0       
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [simplify_mode](#i_simplify_mode)      | 0       
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

- <span id="i_MESH_NORMAL"></span>**MESH_NORMAL** = **0**
- <span id="i_MESH_WIREFRAME"></span>**MESH_WIREFRAME** = **1**
- <span id="i_MESH_DEBUG_OCTREE"></span>**MESH_DEBUG_OCTREE** = **2**
- <span id="i_MESH_DEBUG_DUAL_GRID"></span>**MESH_DEBUG_DUAL_GRID** = **3**

enum **SimplifyMode**: 

- <span id="i_SIMPLIFY_OCTREE_BOTTOM_UP"></span>**SIMPLIFY_OCTREE_BOTTOM_UP** = **0**
- <span id="i_SIMPLIFY_OCTREE_TOP_DOWN"></span>**SIMPLIFY_OCTREE_TOP_DOWN** = **1**
- <span id="i_SIMPLIFY_NONE"></span>**SIMPLIFY_NONE** = **2**

enum **SeamMode**: 

- <span id="i_SEAM_NONE"></span>**SEAM_NONE** = **0**
- <span id="i_SEAM_MARCHING_SQUARE_SKIRTS"></span>**SEAM_MARCHING_SQUARE_SKIRTS** = **1**


## Property Descriptions

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_geometric_error"></span> **geometric_error** = 0

*(This property has no documentation)*

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_mesh_mode"></span> **mesh_mode** = 0

*(This property has no documentation)*

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_seam_mode"></span> **seam_mode** = 0

*(This property has no documentation)*

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_simplify_mode"></span> **simplify_mode** = 0

*(This property has no documentation)*

## Method Descriptions

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_get_geometric_error"></span> **get_geometric_error**( ) 

*(This method has no documentation)*

### [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)<span id="i_get_statistics"></span> **get_statistics**( ) 

*(This method has no documentation)*

### [void](#)<span id="i_set_geometric_error"></span> **set_geometric_error**( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) error ) 

*(This method has no documentation)*

_Generated on Apr 06, 2024_
