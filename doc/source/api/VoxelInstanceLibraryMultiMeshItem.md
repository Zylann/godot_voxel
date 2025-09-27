# VoxelInstanceLibraryMultiMeshItem

Inherits: [VoxelInstanceLibraryItem](VoxelInstanceLibraryItem.md)

Instancer model using [MultiMesh](https://docs.godotengine.org/en/stable/classes/class_multimesh.html) to render.

## Description: 

This model is suited for rendering very large amounts of simple instances, such as grass and rocks.

## Properties: 


Type                                                                                                                                         | Name                                                       | Default                               
-------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------- | --------------------------------------
[PackedFloat32Array](https://docs.godotengine.org/en/stable/classes/class_packedfloat32array.html)                                           | [_mesh_lod_distance_ratios](#i__mesh_lod_distance_ratios)  | PackedFloat32Array(0.2, 0.35, 0.6, 1) 
[ShadowCastingSetting](https://docs.godotengine.org/en/stable/classes/class_renderingserver.html#enum-renderingserver-shadowcastingsetting)  | [cast_shadow](#i_cast_shadow)                              | 1                                     
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                                                                     | [collision_distance](#i_collision_distance)                | -1.0                                  
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                                                                         | [collision_layer](#i_collision_layer)                      | 1                                     
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                                                                         | [collision_mask](#i_collision_mask)                        | 1                                     
[Array](https://docs.godotengine.org/en/stable/classes/class_array.html)                                                                     | [collision_shapes](#i_collision_shapes)                    | []                                    
[GIMode](https://docs.godotengine.org/en/stable/classes/class_geometryinstance3d.html#enum-geometryinstance3d-gimode)                        | [gi_mode](#i_gi_mode)                                      | 1                                     
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)                                                                       | [hide_beyond_max_lod](#i_hide_beyond_max_lod)              | false                                 
[Material](https://docs.godotengine.org/en/stable/classes/class_material.html)                                                               | [material_override](#i_material_override)                  |                                       
[Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)                                                                       | [mesh](#i_mesh)                                            |                                       
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                                                                     | [mesh_lod0_distance_ratio](#i_mesh_lod0_distance_ratio)    | 0.2                                   
[Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)                                                                       | [mesh_lod1](#i_mesh_lod1)                                  |                                       
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                                                                     | [mesh_lod1_distance_ratio](#i_mesh_lod1_distance_ratio)    | 0.35                                  
[Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)                                                                       | [mesh_lod2](#i_mesh_lod2)                                  |                                       
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                                                                     | [mesh_lod2_distance_ratio](#i_mesh_lod2_distance_ratio)    | 0.6                                   
[Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)                                                                       | [mesh_lod3](#i_mesh_lod3)                                  |                                       
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)                                                                     | [mesh_lod3_distance_ratio](#i_mesh_lod3_distance_ratio)    | 1.0                                   
[RemovalBehavior](VoxelInstanceLibraryMultiMeshItem.md#enumerations)                                                                         | [removal_behavior](#i_removal_behavior)                    | REMOVAL_BEHAVIOR_NONE (0)             
[PackedScene](https://docs.godotengine.org/en/stable/classes/class_packedscene.html)                                                         | [removal_scene](#i_removal_scene)                          |                                       
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                                                                         | [render_layer](#i_render_layer)                            | 1                                     
[PackedScene](https://docs.godotengine.org/en/stable/classes/class_packedscene.html)                                                         | [scene](#i_scene)                                          |                                       
<p></p>

## Methods: 


Return                                                                                  | Signature                                                                                                                                                                                                 
--------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                                               | [_on_instance_removed](#i__on_instance_removed) ( [VoxelInstancer](VoxelInstancer.md) instancer, [Transform3D](https://docs.godotengine.org/en/stable/classes/class_transform3d.html) transform ) virtual 
[StringName[]](https://docs.godotengine.org/en/stable/classes/class_stringname[].html)  | [get_collider_group_names](#i_get_collider_group_names) ( ) const                                                                                                                                         
[Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)                  | [get_mesh](#i_get_mesh) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mesh_lod_index ) const                                                                                     
[void](#)                                                                               | [set_collider_group_names](#i_set_collider_group_names) ( [StringName[]](https://docs.godotengine.org/en/stable/classes/class_stringname[].html) names )                                                  
[void](#)                                                                               | [set_mesh](#i_set_mesh) ( [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html) mesh, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mesh_lod_index )              
[void](#)                                                                               | [setup_from_template](#i_setup_from_template) ( [Node](https://docs.godotengine.org/en/stable/classes/class_node.html) node )                                                                             
<p></p>

## Enumerations: 

enum **RemovalBehavior**: 

- <span id="i_REMOVAL_BEHAVIOR_NONE"></span>**REMOVAL_BEHAVIOR_NONE** = **0** --- No extra logic will run when instances are removed.
- <span id="i_REMOVAL_BEHAVIOR_INSTANTIATE"></span>**REMOVAL_BEHAVIOR_INSTANTIATE** = **1** --- Instantiates the [removal_scene](VoxelInstanceLibraryMultiMeshItem.md#i_removal_scene) for each instance getting removed. The scene must have a root derived from [Node3D](https://docs.godotengine.org/en/stable/classes/class_node3d.html), and will be given the same transform as the instance before being added to the scene tree. It will be added under the [VoxelInstancer](VoxelInstancer.md) node.
- <span id="i_REMOVAL_BEHAVIOR_CALLBACK"></span>**REMOVAL_BEHAVIOR_CALLBACK** = **2** --- Calls [_on_instance_removed](VoxelInstanceLibraryMultiMeshItem.md#i__on_instance_removed) when an instance gets removed. You should attach a script to the item in order to implement this. Note: every resource can have a [Object.script](https://docs.godotengine.org/en/stable/classes/class_object.html#class-object-property-script). But in the editor, Godot currently doesn't show you that property if the resource appears in a sub-inspector. To workaround that, right-click the property in which the resource is, and choose "Edit". That will open the item in a full inspector. An alternative is to save your item as a file, and then edit it from the file browser.


## Constants: 

- <span id="i_MAX_MESH_LODS"></span>**MAX_MESH_LODS** = **4**

## Property Descriptions

### [PackedFloat32Array](https://docs.godotengine.org/en/stable/classes/class_packedfloat32array.html)<span id="i__mesh_lod_distance_ratios"></span> **_mesh_lod_distance_ratios** = PackedFloat32Array(0.2, 0.35, 0.6, 1)

*(This property has no documentation)*

### [ShadowCastingSetting](https://docs.godotengine.org/en/stable/classes/class_renderingserver.html#enum-renderingserver-shadowcastingsetting)<span id="i_cast_shadow"></span> **cast_shadow** = 1

*(This property has no documentation)*

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_collision_distance"></span> **collision_distance** = -1.0

When greater than 0 and colliders are configured, hints the distance below which colliders may be created. This allows to reduce the amount of colliders while keeping a high view distance for visuals. 

When negative, colliders will be created at all distances.

Note: the instancer creates/removes colliders on a per-chunk basis, so this distance is compared against the distance to chunks, and not individual instances.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_collision_layer"></span> **collision_layer** = 1

*(This property has no documentation)*

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_collision_mask"></span> **collision_mask** = 1

*(This property has no documentation)*

### [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_collision_shapes"></span> **collision_shapes** = []

Alternating list of [CollisionShape](https://docs.godotengine.org/en/stable/classes/class_collisionshape.html) and [Transform3D](https://docs.godotengine.org/en/stable/classes/class_transform3d.html). Shape comes first, followed by its local transform relative to the instance. Setting up collision shapes in the editor may require using a scene instead.

### [GIMode](https://docs.godotengine.org/en/stable/classes/class_geometryinstance3d.html#enum-geometryinstance3d-gimode)<span id="i_gi_mode"></span> **gi_mode** = 1

*(This property has no documentation)*

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_hide_beyond_max_lod"></span> **hide_beyond_max_lod** = false

*(This property has no documentation)*

### [Material](https://docs.godotengine.org/en/stable/classes/class_material.html)<span id="i_material_override"></span> **material_override**

*(This property has no documentation)*

### [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_mesh"></span> **mesh**

*(This property has no documentation)*

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_mesh_lod0_distance_ratio"></span> **mesh_lod0_distance_ratio** = 0.2

*(This property has no documentation)*

### [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_mesh_lod1"></span> **mesh_lod1**

*(This property has no documentation)*

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_mesh_lod1_distance_ratio"></span> **mesh_lod1_distance_ratio** = 0.35

*(This property has no documentation)*

### [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_mesh_lod2"></span> **mesh_lod2**

*(This property has no documentation)*

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_mesh_lod2_distance_ratio"></span> **mesh_lod2_distance_ratio** = 0.6

*(This property has no documentation)*

### [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_mesh_lod3"></span> **mesh_lod3**

*(This property has no documentation)*

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_mesh_lod3_distance_ratio"></span> **mesh_lod3_distance_ratio** = 1.0

*(This property has no documentation)*

### [RemovalBehavior](VoxelInstanceLibraryMultiMeshItem.md#enumerations)<span id="i_removal_behavior"></span> **removal_behavior** = REMOVAL_BEHAVIOR_NONE (0)

Specifies what should happen when instances get removed. This is useful if they should turn into more complex objects with animation or logic in them.

### [PackedScene](https://docs.godotengine.org/en/stable/classes/class_packedscene.html)<span id="i_removal_scene"></span> **removal_scene**

Scene that will be used if [removal_behavior](VoxelInstanceLibraryMultiMeshItem.md#i_removal_behavior) is set to [REMOVAL_BEHAVIOR_INSTANTIATE](VoxelInstanceLibraryMultiMeshItem.md#i_REMOVAL_BEHAVIOR_INSTANTIATE).

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_render_layer"></span> **render_layer** = 1

*(This property has no documentation)*

### [PackedScene](https://docs.godotengine.org/en/stable/classes/class_packedscene.html)<span id="i_scene"></span> **scene**

Scene that will be used as configuration instead of manual properties. It should have a specific node structure to be supported. See [https://voxel-tools.readthedocs.io/en/latest/instancing/#setting-up-a-multimesh-item-from-a-scene](https://voxel-tools.readthedocs.io/en/latest/instancing/#setting-up-a-multimesh-item-from-a-scene)

## Method Descriptions

### [void](#)<span id="i__on_instance_removed"></span> **_on_instance_removed**( [VoxelInstancer](VoxelInstancer.md) instancer, [Transform3D](https://docs.godotengine.org/en/stable/classes/class_transform3d.html) transform ) 

This method will be called if you set [removal_behavior](VoxelInstanceLibraryMultiMeshItem.md#i_removal_behavior) to [REMOVAL_BEHAVIOR_CALLBACK](VoxelInstanceLibraryMultiMeshItem.md#i_REMOVAL_BEHAVIOR_CALLBACK).

Note: this method can be called from within the removal of a node that is child of [VoxelInstancer](VoxelInstancer.md). In this context, Godot will prevent you from adding new child nodes. You can workaround that by using [Object.call_deferred](https://docs.godotengine.org/en/stable/classes/class_object.html#class-object-method-call-deferred). See also [VoxelInstancerRigidBody.queue_free_and_notify_instancer](VoxelInstancerRigidBody.md#i_queue_free_and_notify_instancer).

### [StringName[]](https://docs.godotengine.org/en/stable/classes/class_stringname[].html)<span id="i_get_collider_group_names"></span> **get_collider_group_names**( ) 

Gets the list of group names that are added to collider nodes.

### [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_get_mesh"></span> **get_mesh**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mesh_lod_index ) 

*(This method has no documentation)*

### [void](#)<span id="i_set_collider_group_names"></span> **set_collider_group_names**( [StringName[]](https://docs.godotengine.org/en/stable/classes/class_stringname[].html) names ) 

Sets the list of group names that will be added to collider nodes generated for each instance.

### [void](#)<span id="i_set_mesh"></span> **set_mesh**( [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html) mesh, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) mesh_lod_index ) 

*(This method has no documentation)*

### [void](#)<span id="i_setup_from_template"></span> **setup_from_template**( [Node](https://docs.godotengine.org/en/stable/classes/class_node.html) node ) 

*(This method has no documentation)*

_Generated on Aug 09, 2025_
