# VoxelMeshSDF

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)

Signed distance field of a mesh as voxels.

## Description: 

This resource can be used to bake a mesh into a signed distance field (SDF). The data is stored as an internal voxel buffer. This can then be used as a shape or brush in voxel-based operations.


Note: if you can, prefer using procedural shapes (like sphere or box) for better quality and performance. While versatile, [VoxelMeshSDF](VoxelMeshSDF.md) it is more expensive to use and currently has lower quality compared to procedural equivalents.


Note 2: not all meshes can be baked. Best meshes should be manifold and represent a closed shape, with clear inside and outside.

## Properties: 


Type                                                                                | Name                                                       | Default 
----------------------------------------------------------------------------------- | ---------------------------------------------------------- | --------
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)  | [_data](#i__data)                                          | {}      
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [bake_mode](#i_bake_mode)                                  | 1       
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)              | [boundary_sign_fix_enabled](#i_boundary_sign_fix_enabled)  | true    
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [cell_count](#i_cell_count)                                | 64      
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)            | [margin_ratio](#i_margin_ratio)                            | 0.25    
[Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)              | [mesh](#i_mesh)                                            |         
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [partition_subdiv](#i_partition_subdiv)                    | 32      
<p></p>

## Methods: 


Return                                                                    | Signature                                                                                                                    
------------------------------------------------------------------------- | -----------------------------------------------------------------------------------------------------------------------------
[void](#)                                                                 | [bake](#i_bake) ( )                                                                                                          
[void](#)                                                                 | [bake_async](#i_bake_async) ( [SceneTree](https://docs.godotengine.org/en/stable/classes/class_scenetree.html) scene_tree )  
[Array](https://docs.godotengine.org/en/stable/classes/class_array.html)  | [debug_check_sdf](#i_debug_check_sdf) ( [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html) mesh )        
[AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html)    | [get_aabb](#i_get_aabb) ( ) const                                                                                            
[VoxelBuffer](VoxelBuffer.md)                                             | [get_voxel_buffer](#i_get_voxel_buffer) ( ) const                                                                            
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)    | [is_baked](#i_is_baked) ( ) const                                                                                            
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)    | [is_baking](#i_is_baking) ( ) const                                                                                          
<p></p>

## Signals: 

### baked( ) 

Emitted when asynchronous baking is complete.

## Enumerations: 

enum **BakeMode**: 

- <span id="i_BAKE_MODE_ACCURATE_NAIVE"></span>**BAKE_MODE_ACCURATE_NAIVE** = **0** --- Checks every triangle from every cell to find the closest distances. It's accurate, but much slower than other techniques. The mesh must be closed, otherwise the SDF will contain errors. Signs are calculated by doing several raycasts from the center of each cell: if the ray hits a backface, the cell is assumed to be inside. Otherwise, it is assumed to be outside.
- <span id="i_BAKE_MODE_ACCURATE_PARTITIONED"></span>**BAKE_MODE_ACCURATE_PARTITIONED** = **1** --- Similar to the naive method, but subdivides space into partitions in order to more easily skip triangles that don't need to be checked. Faster than the naive method, but still relatively slow.
- <span id="i_BAKE_MODE_APPROX_INTERP"></span>**BAKE_MODE_APPROX_INTERP** = **2** --- Experimental method subdividing space in 4x4x4 cells, and skipping SDF calculations by interpolating 8 corners instead if no triangles are present in those cells. Faster than the naive method, but not particularly interesting. Might be removed.
- <span id="i_BAKE_MODE_APPROX_FLOODFILL"></span>**BAKE_MODE_APPROX_FLOODFILL** = **3** --- Approximates the SDF by calculating a thin "hull" of accurate values near triangles, then propagates those values with a 26-way floodfill. While technically not accurate, it is currently the fastest method and results are often good enough.
- <span id="i_BAKE_MODE_COUNT"></span>**BAKE_MODE_COUNT** = **4** --- How many baking modes there are.


## Property Descriptions

### [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)<span id="i__data"></span> **_data** = {}

*(This property has no documentation)*

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_bake_mode"></span> **bake_mode** = 1

Selects the algorithm that will be used to compute SDF from the mesh.

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_boundary_sign_fix_enabled"></span> **boundary_sign_fix_enabled** = true

Some algorithms might fail to properly compute the sign of distances on their own. Usually outside the shape has positive sign, and inside the shape has negative sign. Errors can cause the wrong sign to "leak" and propagate outside the shape.

This option attempts to fix this when it happens by assuming that the border of the baked region is always outside, and propagates positive signs inwards to ensure no negative signs leak.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_cell_count"></span> **cell_count** = 64

Controls the resolution of the baked SDF, proportionally to one side of the 3D voxel buffer. Higher increases quality, but is slower to bake and uses more memory.

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_margin_ratio"></span> **margin_ratio** = 0.25

Controls the addition of extra space around the mesh's baking area.

Proper SDF buffers should have some margin in order to capture outside gradients. Without margin, the outside could appear cutoff or blocky when remeshed.

This property adds margin based on a ratio of the size of the mesh. For example, if the mesh is 100 units wide, a margin of 0.1 will add 10 extra units of space around it.

### [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_mesh"></span> **mesh**

Mesh that will be baked when calling the [VoxelMeshSDF.bake](VoxelMeshSDF.md#i_bake) function, or when pressing the Bake button in the editor.

Setting this property back to null will not erase the baked result, so you don't need the original mesh to be loaded in order to use the SDF.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_partition_subdiv"></span> **partition_subdiv** = 32

Controls how many subdivisions to use across the baking area when using the BAKE_MODE_ACCURATE_PARTITIONED mode.

## Method Descriptions

### [void](#)<span id="i_bake"></span> **bake**( ) 

Bakes the SDF on the calling thread, using the currently assigned [VoxelMeshSDF.mesh](VoxelMeshSDF.md#i_mesh). Might cause the game to stall if done on the main thread.

### [void](#)<span id="i_bake_async"></span> **bake_async**( [SceneTree](https://docs.godotengine.org/en/stable/classes/class_scenetree.html) scene_tree ) 

Bakes the SDF on a separate thread, using the currently assigned [VoxelMeshSDF.mesh](VoxelMeshSDF.md#i_mesh). See also [VoxelMeshSDF.is_baking](VoxelMeshSDF.md#i_is_baking), [VoxelMeshSDF.is_baked](VoxelMeshSDF.md#i_is_baked) and [VoxelMeshSDF.baked](VoxelMeshSDF.md#signals).

### [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_debug_check_sdf"></span> **debug_check_sdf**( [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html) mesh ) 

Experimental.

Runs some checks to verify if the baked SDF contains errors. The mesh to pass should be the mesh that was used for baking.

If there are errors, the returned array will contain information about up to 2 cells of the buffer containing wrong values, in the following format:

```
[
	cell0_position: Vector3,
	cell0_triangle_v0: Vector3,
	cell0_triangle_v1: Vector3,
	cell0_triangle_v2: Vector3,
	cell1_position: Vector3,
	cell1_triangle_v0: Vector3,
	cell1_triangle_v1: Vector3,
	cell1_triangle_v2: Vector3,
]
```
If there are no errors, the returned array will be empty. 

This method is a leftover from when this resource was initially implemented, as an attempt to automate checks when debugging. Errors might not always indicate an unusable SDF, depending on the use case. It might be removed or changed in the future.

### [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html)<span id="i_get_aabb"></span> **get_aabb**( ) 

Get the reference bounding box of the baked shape. This may be a bit larger than the original mesh's AABB because of the [VoxelMeshSDF.margin](VoxelMeshSDF.md#i_margin) property.

### [VoxelBuffer](VoxelBuffer.md)<span id="i_get_voxel_buffer"></span> **get_voxel_buffer**( ) 

Get the [VoxelBuffer](VoxelBuffer.md) containing the baked distance field. Results will be stored in the SDF channel.

This buffer should not be modified.

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_is_baked"></span> **is_baked**( ) 

Gets whether the resource contains baked SDF data.

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_is_baking"></span> **is_baking**( ) 

Gets whether a asynchronous baking operation is pending.

_Generated on Apr 06, 2024_
