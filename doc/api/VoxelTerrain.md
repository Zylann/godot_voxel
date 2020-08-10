# Class: VoxelTerrain

Inherits: Spatial

_Godot version: 3.2_


## Online Tutorials: 



## Constants:


## Properties:

#### » bool generate_collisions

`set_generate_collisions (value)` setter

`get_generate_collisions ()` getter


#### » VoxelStream stream

`set_stream (value)` setter

`get_stream ()` getter


#### » int view_distance

`set_view_distance (value)` setter

`get_view_distance ()` getter


#### » NodePath viewer_path

`set_viewer_path (value)` setter

`get_viewer_path ()` getter


#### » VoxelLibrary voxel_library

`set_voxel_library (value)` setter

`get_voxel_library ()` getter



## Methods:

#### » Vector3 block_to_voxel ( Vector3 block_pos ) 


#### » Material get_material ( int id )  const


#### » Dictionary get_statistics (  )  const


#### » VoxelTool get_voxel_tool (  ) 


#### » void save_block ( Vector3 arg0 ) 


#### » void save_modified_blocks (  ) 


#### » void set_material ( int id, Material material ) 


#### » Vector3 voxel_to_block ( Vector3 voxel_pos ) 



## Signals:

#### » block_loaded
Emitted when a new block is loaded from stream. Note it might be not visible yet.



#### » block_unloaded
Emitted when a block unloaded due to being outside view distance.




---
* [Class List](Class_List.md)
* [Doc Index](../01_get-started.md)

_Generated on Aug 10, 2020_
