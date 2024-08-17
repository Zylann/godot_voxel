# VoxelRaycastResult

Inherits: [RefCounted](https://docs.godotengine.org/en/stable/classes/class_refcounted.html)

Result of a raycast performed with [VoxelTool.raycast](VoxelTool.md#i_raycast)

## Properties: 


Type                                                                            | Name                                       | Default           
------------------------------------------------------------------------------- | ------------------------------------------ | ------------------
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)        | [distance](#i_distance)                    | 0.0               
[Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)  | [position](#i_position)                    | Vector3i(0, 0, 0) 
[Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)  | [previous_position](#i_previous_position)  | Vector3i(0, 0, 0) 
<p></p>

## Property Descriptions

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_distance"></span> **distance** = 0.0

*(This property has no documentation)*

### [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)<span id="i_position"></span> **position** = Vector3i(0, 0, 0)

Integer position of the voxel that was hit.

### [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)<span id="i_previous_position"></span> **previous_position** = Vector3i(0, 0, 0)

Integer position of the previous voxel along the ray before the final hit.

_Generated on Apr 06, 2024_
