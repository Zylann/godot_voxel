# VoxelAStarGrid3D

Inherits: [RefCounted](https://docs.godotengine.org/en/stable/classes/class_refcounted.html)

!!! warning
    This class is marked as experimental. It is subject to likely change or possible removal in future versions. Use at your own discretion.

Grid-based A* pathfinding algorithm adapted to blocky voxel terrain.

## Description: 

This can be used to find paths between two voxel positions on blocky terrain.

It is tuned for agents 2 voxels tall and 1 voxel wide, which must stand on solid voxels and can jump 1 voxel high.

Search radius may also be limited (50 voxels and above starts to be relatively expensive).

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

### async_search_completed( [Vector3i[]](https://docs.godotengine.org/en/stable/classes/class_vector3i[].html) name ) 

*(This signal has no documentation)*

## Method Descriptions

### [Vector3i[]](https://docs.godotengine.org/en/stable/classes/class_vector3i[].html)<span id="i_debug_get_visited_positions"></span> **debug_get_visited_positions**( ) 

*(This method has no documentation)*

### [Vector3i[]](https://docs.godotengine.org/en/stable/classes/class_vector3i[].html)<span id="i_find_path"></span> **find_path**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) from_position, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) to_position ) 

*(This method has no documentation)*

### [void](#)<span id="i_find_path_async"></span> **find_path_async**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) from_position, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) to_position ) 

*(This method has no documentation)*

### [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html)<span id="i_get_region"></span> **get_region**( ) 

*(This method has no documentation)*

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_is_running_async"></span> **is_running_async**( ) 

*(This method has no documentation)*

### [void](#)<span id="i_set_region"></span> **set_region**( [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) box ) 

*(This method has no documentation)*

### [void](#)<span id="i_set_terrain"></span> **set_terrain**( [VoxelTerrain](VoxelTerrain.md) terrain ) 

*(This method has no documentation)*

_Generated on Apr 06, 2024_
