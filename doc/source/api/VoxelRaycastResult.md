# VoxelRaycastResult

Inherits: [RefCounted](https://docs.godotengine.org/en/stable/classes/class_refcounted.html)

Result of a raycast performed with [VoxelTool.raycast](VoxelTool.md#i_raycast)

## Properties: 


Type                                                                            | Name                                       | Default           
------------------------------------------------------------------------------- | ------------------------------------------ | ------------------
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)        | [distance](#i_distance)                    | 0.0               
[Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)    | [normal](#i_normal)                        | Vector3(0, 0, 0)  
[Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)  | [position](#i_position)                    | Vector3i(0, 0, 0) 
[Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)  | [previous_position](#i_previous_position)  | Vector3i(0, 0, 0) 
<p></p>

## Property Descriptions

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_distance"></span> **distance** = 0.0

Distance between the origin of the ray and the surface of the cube representing the hit voxel.

### [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)<span id="i_normal"></span> **normal** = Vector3(0, 0, 0)

Unit vector pointing away from the surface that was hit.

This is only available if [VoxelTool](VoxelTool.md) was configured to compute normals. See [VoxelTool.set_raycast_normal_enabled](VoxelTool.md#i_set_raycast_normal_enabled).

With blocky voxels, this normal will be based on collision boxes rather than the mesh.

### [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)<span id="i_position"></span> **position** = Vector3i(0, 0, 0)

Integer position of the voxel that was hit. In a blocky game, this would be the position of the voxel to interact with.

### [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)<span id="i_previous_position"></span> **previous_position** = Vector3i(0, 0, 0)

Integer position of the previous voxel along the ray before the final hit. In a blocky game, this would be the position of the voxel to place on top of the pointed one.

_Generated on Aug 09, 2025_
