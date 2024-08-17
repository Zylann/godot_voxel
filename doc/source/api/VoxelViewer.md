# VoxelViewer

Inherits: [Node3D](https://docs.godotengine.org/en/stable/classes/class_node3d.html)

Attach this as a child node of characters, so the voxel world will know where to load blocks around them.

If no viewer is present in the world, nothing will generate.

## Description: 

The voxel world uses the position and options of all the [VoxelViewer](VoxelViewer.md) nodes to determine where to load blocks, and prioritize updates. For example, a voxel placed 100 units away from a player will have much lower priority than the modifications that player is doing when digging in front of them.

## Properties: 


Type                                                                    | Name                                                                       | Default 
----------------------------------------------------------------------- | -------------------------------------------------------------------------- | --------
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)  | [enabled_in_editor](#i_enabled_in_editor)                                  | false   
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)  | [requires_collisions](#i_requires_collisions)                              | true    
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)  | [requires_data_block_notifications](#i_requires_data_block_notifications)  | false   
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)  | [requires_visuals](#i_requires_visuals)                                    | true    
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)    | [view_distance](#i_view_distance)                                          | 128     
<p></p>

## Methods: 


Return                                                                | Signature                                                                                                                  
--------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [get_network_peer_id](#i_get_network_peer_id) ( ) const                                                                    
[void](#)                                                             | [set_network_peer_id](#i_set_network_peer_id) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) id )  
<p></p>

## Property Descriptions

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_enabled_in_editor"></span> **enabled_in_editor** = false

Sets whether this viewer will cause loading to occur in the editor. This is mainly intented for testing purposes.

Note that streaming in editor can also be turned off on terrains.

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_requires_collisions"></span> **requires_collisions** = true

If set to `true`, the engine will generate classic collision shapes around this viewer.

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_requires_data_block_notifications"></span> **requires_data_block_notifications** = false

*(This property has no documentation)*

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_requires_visuals"></span> **requires_visuals** = true

If set to `true`, the engine will generate meshes around this viewer. This may be enabled for the local player.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_view_distance"></span> **view_distance** = 128

How far should voxels generate around this viewer.

## Method Descriptions

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_network_peer_id"></span> **get_network_peer_id**( ) 

*(This method has no documentation)*

### [void](#)<span id="i_set_network_peer_id"></span> **set_network_peer_id**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) id ) 

*(This method has no documentation)*

_Generated on Apr 06, 2024_
