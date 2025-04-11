# VoxelAStarGrid3D

Inherits: [RefCounted](https://docs.godotengine.org/en/stable/classes/class_refcounted.html)

!!! warning
    This class is marked as experimental. It is subject to likely change or possible removal in future versions. Use at your own discretion.

Grid-based A* pathfinding algorithm adapted to blocky voxel terrain.

## Description: 

This can be used to find paths between two voxel positions on blocky terrain.

It is tuned for agents 2 voxels tall and 1 voxel wide, which must stand on solid voxels and can jump 1 voxel high.

No navmesh is required, it uses voxels directly with no baking. However, search radius is limited by an area (50 voxels and above starts to be relatively expensive).

At the moment, this pathfinder only considers voxels with ID 0 to be air, and the rest is considered solid.

Note: "positions" in this class are expected to be in voxels. If your terrain is offset or if voxels are smaller or bigger than world units, you may have to convert coordinates.

## Methods: 


Return                                                                              | Signature                                                                                                                                                                                                                           
----------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[Vector3i[]](https://docs.godotengine.org/en/stable/classes/class_vector3i[].html)  | [debug_get_visited_positions](#i_debug_get_visited_positions) ( ) const                                                                                                                                                             
[Vector3i[]](https://docs.godotengine.org/en/stable/classes/class_vector3i[].html)  | [find_path](#i_find_path) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) from_position, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) to_position )              
[void](#)                                                                           | [find_path_async](#i_find_path_async) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) from_position, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) to_position )  
[AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html)              | [get_region](#i_get_region) ( )                                                                                                                                                                                                     
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)              | [is_running_async](#i_is_running_async) ( ) const                                                                                                                                                                                   
[void](#)                                                                           | [set_region](#i_set_region) ( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) box )                                                                                                                          
[void](#)                                                                           | [set_terrain](#i_set_terrain) ( [VoxelTerrain](VoxelTerrain.md) terrain )                                                                                                                                                           
<p></p>

## Signals: 

### async_search_completed( [Vector3i[]](https://docs.godotengine.org/en/stable/classes/class_vector3i[].html) path ) 

Emitted when searches triggered with [VoxelAStarGrid3D.find_path_async](VoxelAStarGrid3D.md#i_find_path_async) are complete.

## Method Descriptions

### [Vector3i[]](https://docs.godotengine.org/en/stable/classes/class_vector3i[].html)<span id="i_debug_get_visited_positions"></span> **debug_get_visited_positions**( ) 

Gets the list of voxel positions that were visited by the last pathfinding request (relates to how A* works under the hood). This is for debugging.

### [Vector3i[]](https://docs.godotengine.org/en/stable/classes/class_vector3i[].html)<span id="i_find_path"></span> **find_path**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) from_position, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) to_position ) 

Calculates a path starting from a voxel position to a target voxel position. 

Those positions should be air voxels just above ground with enough room for agents to fit in. 

The returned path will be a series of contiguous voxel positions to walk through in order to get to the destination. 

If no path is found, or if either start or destination position is outside of the search area, an empty array will be returned.

You may also use [VoxelAStarGrid3D.set_region](VoxelAStarGrid3D.md#i_set_region) to specify the search area.

### [void](#)<span id="i_find_path_async"></span> **find_path_async**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) from_position, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) to_position ) 

Same as [VoxelAStarGrid3D.find_path](VoxelAStarGrid3D.md#i_find_path), but performs the calculation on a separate thread. The result will be emitted with the [VoxelAStarGrid3D.async_search_completed](VoxelAStarGrid3D.md#signals) signal.

Only one asynchronous search can be active at a given time. Use [VoxelAStarGrid3D.is_running_async](VoxelAStarGrid3D.md#i_is_running_async) to check this.

### [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html)<span id="i_get_region"></span> **get_region**( ) 

Gets the maximum region limit that will be considered for pathfinding, in voxels.

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_is_running_async"></span> **is_running_async**( ) 

Returns true if a path is currently being calculated asynchronously. See [VoxelAStarGrid3D.find_path_async](VoxelAStarGrid3D.md#i_find_path_async).

### [void](#)<span id="i_set_region"></span> **set_region**( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) box ) 

Sets the maximum region limit that will be considered for pathfinding, in voxels. You should usually set this before calling [VoxelAStarGrid3D.find_path](VoxelAStarGrid3D.md#i_find_path).

The larger the region, the more expensive the search can get. Keep in mind voxel volumes scale cubically, so don't use this on large areas (for example 50 voxels is quite big).

### [void](#)<span id="i_set_terrain"></span> **set_terrain**( [VoxelTerrain](VoxelTerrain.md) terrain ) 

Sets the terrain that will be used to do searches in.

_Generated on Mar 23, 2025_
