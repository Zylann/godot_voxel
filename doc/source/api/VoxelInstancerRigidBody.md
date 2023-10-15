# VoxelInstancerRigidBody

Inherits: [RigidBody3D](https://docs.godotengine.org/en/stable/classes/class_rigidbody3d.html)

Collision node generated for every collidable multimesh instance created by [VoxelInstancer](VoxelInstancer.md).

## Description: 

Instances generated from [VoxelInstanceLibraryMultiMeshItem](VoxelInstanceLibraryMultiMeshItem.md) don't use nodes to render. However, they can be given collision, in the form of body nodes using this class.

Calling `queue_free()` on an instance of this node will also unregister the instance from [VoxelInstancer](VoxelInstancer.md).

## Methods: 


Return                                                                | Signature                                               
--------------------------------------------------------------------- | --------------------------------------------------------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [get_library_item_id](#i_get_library_item_id) ( ) const 
<p></p>

## Method Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_library_item_id"></span> **get_library_item_id**( ) 

Gets the ID of the item in the instancer's [VoxelInstanceLibrary](VoxelInstanceLibrary.md) that was used to create the instance having this collider.

_Generated on Oct 15, 2023_
