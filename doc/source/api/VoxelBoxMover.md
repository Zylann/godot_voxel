# VoxelBoxMover

Inherits: [Reference](https://docs.godotengine.org/en/stable/classes/class_reference.html)




## Description: 

Utility class allowing to reproduce simple move-and-slide logic using only voxel AABBs, similar to Minecraft physics. This class may only be used with blocky voxels.

Store an instance of it within a member variable of your script, and use it within method Node._process or method Node._physics_process (it works wherever you like).

```gdscript
var motion = Vector3(0, 0, -10 * delta) # Move forward
motion = _box_mover.get_motion(get_translation(), motion, aabb, terrain_node)
global_translate(motion)

```

## Methods: 


Return                                                                        | Signature                                                                                                                                                                                                                                                                                                                                                           
----------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)          | [get_collision_mask](#i_get_collision_mask) ( ) const                                                                                                                                                                                                                                                                                                               
[Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)  | [get_motion](#i_get_motion) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) motion, [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) aabb, [Node](https://docs.godotengine.org/en/stable/classes/class_node.html) terrain )  
[void](#)                                                                     | [set_collision_mask](#i_set_collision_mask) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mask )                                                                                                                                                                                                                                           
<p></p>

## Method Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_collision_mask"></span> **get_collision_mask**( ) 


- [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)<span id="i_get_motion"></span> **get_motion**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) motion, [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) aabb, [Node](https://docs.godotengine.org/en/stable/classes/class_node.html) terrain ) 

Given a motion vector, returns a modified vector telling you by how much to move your character. This is similar to method KinematicBody.move_and_slide, except you have to apply the movement.

- [void](#)<span id="i_set_collision_mask"></span> **set_collision_mask**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mask ) 


_Generated on Nov 06, 2021_
