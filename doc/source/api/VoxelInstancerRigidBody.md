# VoxelInstancerRigidBody

Inherits: [RigidBody3D](https://docs.godotengine.org/en/stable/classes/class_rigidbody3d.html)

Collision node generated for every collidable multimesh instance created by [VoxelInstancer](VoxelInstancer.md).

## Description: 

Instances generated from [VoxelInstanceLibraryMultiMeshItem](VoxelInstanceLibraryMultiMeshItem.md) don't use nodes to render. However, they can be given collision, in the form of body nodes using this class.

Calling `queue_free()` on an instance of this node will also unregister the instance from [VoxelInstancer](VoxelInstancer.md).

## Methods: 


Return                                                                | Signature                                                                  
--------------------------------------------------------------------- | ---------------------------------------------------------------------------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [get_library_item_id](#i_get_library_item_id) ( ) const                    
[void](#)                                                             | [queue_free_and_notify_instancer](#i_queue_free_and_notify_instancer) ( )  
<p></p>

## Method Descriptions

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_library_item_id"></span> **get_library_item_id**( ) 

Gets the ID of the item in the instancer's [VoxelInstanceLibrary](VoxelInstanceLibrary.md) that was used to create the instance having this collider.

### [void](#)<span id="i_queue_free_and_notify_instancer"></span> **queue_free_and_notify_instancer**( ) 

Alternative to `queue_free` in case you don't want to use `call_deferred` to add nodes under [VoxelInstancer](VoxelInstancer.md) from [VoxelInstanceLibraryMultiMeshItem._on_instance_removed](VoxelInstanceLibraryMultiMeshItem.md#i__on_instance_removed).

_Generated on Aug 09, 2025_
