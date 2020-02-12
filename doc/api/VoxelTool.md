# Class: VoxelTool

Inherits: Reference

_Godot version: 3.2.1_


## Online Tutorials: 



## Constants:

#### » Mode.MODE_ADD = 0


#### » Mode.MODE_REMOVE = 1


#### » Mode.MODE_SET = 2



## Properties:

#### » int channel

`set_channel (value)` setter

`get_channel ()` getter


#### » int eraser_value

`set_eraser_value (value)` setter

`get_eraser_value ()` getter


#### » int VoxelTool.Mode.mode

`set_mode (value)` setter

`get_mode ()` getter


#### » int value

`set_value (value)` setter

`get_value ()` getter



## Methods:

#### » void do_point ( Vector3 pos ) 


#### » void do_sphere ( Vector3 center, float radius ) 


#### » int get_voxel ( Vector3 pos ) 


#### » float get_voxel_f ( Vector3 pos ) 


#### » VoxelRaycastResult raycast ( Vector3 origin, Vector3 direction, float max_distance=10.0 ) 


#### » void set_voxel ( Vector3 pos, int v ) 


#### » void set_voxel_f ( Vector3 pos, float v ) 



## Signals:


---
* [Class List](Class_List.md)
* [Doc Index](../01_get-started.md)

_Generated on Feb 13, 2020_
