# Class: VoxelTerrain

Inherits: Spatial

_Godot version: 3.2_


## Online Tutorials: 



## Constants:


## Properties:

#### » bool generate_collisions

`set_generate_collisions (value)` setter

`get_generate_collisions ()` getter


#### » bool smooth_meshing_enabled

`set_smooth_meshing_enabled (value)` setter

`is_smooth_meshing_enabled ()` getter


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


#### » VoxelMap get_storage (  ) 


#### » void make_area_dirty ( AABB aabb ) 


#### » void make_voxel_dirty ( Vector3 pos ) 


#### » Variant raycast ( Vector3 origin, Vector3 direction, float max_distance=100 ) 


#### » void set_material ( int id, Material material ) 


#### » Vector3 voxel_to_block ( Vector3 voxel_pos ) 



## Signals:


---
* [Class List](Class_List.md)
* [Doc Index](../01_get-started.md)

_Generated on Aug 26, 2019_
