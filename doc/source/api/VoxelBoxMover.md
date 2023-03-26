# VoxelBoxMover

Inherits: [RefCounted](https://docs.godotengine.org/en/stable/classes/class_refcounted.html)




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
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)      | [get_max_step_height](#i_get_max_step_height) ( ) const                                                                                                                                                                                                                                                                                                             
[Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)  | [get_motion](#i_get_motion) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) motion, [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) aabb, [Node](https://docs.godotengine.org/en/stable/classes/class_node.html) terrain )  
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)        | [has_stepped_up](#i_has_stepped_up) ( ) const                                                                                                                                                                                                                                                                                                                       
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)        | [is_step_climbing_enabled](#i_is_step_climbing_enabled) ( ) const                                                                                                                                                                                                                                                                                                   
[void](#)                                                                     | [set_collision_mask](#i_set_collision_mask) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mask )                                                                                                                                                                                                                                           
[void](#)                                                                     | [set_max_step_height](#i_set_max_step_height) ( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) height )                                                                                                                                                                                                                                   
[void](#)                                                                     | [set_step_climbing_enabled](#i_set_step_climbing_enabled) ( [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled )                                                                                                                                                                                                                        
<p></p>

## Method Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_collision_mask"></span> **get_collision_mask**( ) 

Gets the collision mask used to detect collidable voxels.

This collision mask is specific to this collision system, and is defined in member VoxelBlockyModel.collision_mask.

- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_get_max_step_height"></span> **get_max_step_height**( ) 


- [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)<span id="i_get_motion"></span> **get_motion**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) pos, [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) motion, [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html) aabb, [Node](https://docs.godotengine.org/en/stable/classes/class_node.html) terrain ) 

Given a motion vector, returns a modified vector telling you by how much to move your character. This is similar to method KinematicBody.move_and_slide, except you have to apply the movement.

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_has_stepped_up"></span> **has_stepped_up**( ) 

When step climbing is enabled, tells when the last call to method get_motion caused climbing to occur.

Climbing modifies the motion vector upwards so that the body is snapped on top of the step. This can have implications in character controller code, such as considering the character to be on the floor instead of having jumped.

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_is_step_climbing_enabled"></span> **is_step_climbing_enabled**( ) 

Tells if step climbing is enabled.

- [void](#)<span id="i_set_collision_mask"></span> **set_collision_mask**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mask ) 

Sets the collision mask used to detect collidable voxels.

Only voxels sharing at least one bit between the masks will be detected.

This collision mask is specific to this collision system, and is defined in member VoxelBlockyModel.collision_mask.

- [void](#)<span id="i_set_max_step_height"></span> **set_max_step_height**( [float](https://docs.godotengine.org/en/stable/classes/class_float.html) height ) 

Sets the maximum height that can be climbed like "stairs".

- [void](#)<span id="i_set_step_climbing_enabled"></span> **set_step_climbing_enabled**( [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled ) 

When enabled, method get_motion will attempt to climb up small steps. This allows to implement Minecraft-like stairs.

_Generated on Mar 26, 2023_
