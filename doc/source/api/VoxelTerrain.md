# VoxelTerrain

Inherits: [VoxelNode](VoxelNode.md)


Voxel volume using constant level of detail.

## Properties: 


Type        | Name                                                                     | Default                                                                               
----------- | ------------------------------------------------------------------------ | --------------------------------------------------------------------------------------
`bool`      | [area_edit_notification_enabled](#i_area_edit_notification_enabled)      | false                                                                                 
`bool`      | [automatic_loading_enabled](#i_automatic_loading_enabled)                | true                                                                                  
`bool`      | [block_enter_notification_enabled](#i_block_enter_notification_enabled)  | false                                                                                 
`AABB`      | [bounds](#i_bounds)                                                      | AABB(-5.36871e+08, -5.36871e+08, -5.36871e+08, 1.07374e+09, 1.07374e+09, 1.07374e+09) 
`int`       | [collision_layer](#i_collision_layer)                                    | 1                                                                                     
`float`     | [collision_margin](#i_collision_margin)                                  | 0.04                                                                                  
`int`       | [collision_mask](#i_collision_mask)                                      | 1                                                                                     
`bool`      | [generate_collisions](#i_generate_collisions)                            | true                                                                                  
`Material`  | [material_override](#i_material_override)                                |                                                                                       
`int`       | [max_view_distance](#i_max_view_distance)                                | 128                                                                                   
`int`       | [mesh_block_size](#i_mesh_block_size)                                    | 16                                                                                    
`bool`      | [run_stream_in_editor](#i_run_stream_in_editor)                          | true                                                                                  
<p></p>

## Methods: 


Return                                                                                                              | Signature                                                                                                                                                                                                                                                                    
------------------------------------------------------------------------------------------------------------------- | -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                                                                           | [_on_area_edited](#i__on_area_edited) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) area_origin, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) area_size ) virtual                                       
[void](#)                                                                                                           | [_on_data_block_entered](#i__on_data_block_entered) ( [VoxelDataBlockEnterInfo](VoxelDataBlockEnterInfo.md) info ) virtual                                                                                                                                                   
[Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)                                      | [data_block_to_voxel](#i_data_block_to_voxel) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) block_pos ) const                                                                                                                             
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                                                | [get_data_block_size](#i_get_data_block_size) ( ) const                                                                                                                                                                                                                      
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)                                  | [get_statistics](#i_get_statistics) ( ) const                                                                                                                                                                                                                                
[PackedInt32Array](https://docs.godotengine.org/en/stable/classes/class_packedint32array.html)                      | [get_viewer_network_peer_ids_in_area](#i_get_viewer_network_peer_ids_in_area) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) area_origin, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) area_size ) const 
[VoxelTool](VoxelTool.md)                                                                                           | [get_voxel_tool](#i_get_voxel_tool) ( )                                                                                                                                                                                                                                      
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)                                              | [has_data_block](#i_has_data_block) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) block_position ) const                                                                                                                                  
[void](#)                                                                                                           | [save_block](#i_save_block) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) position )                                                                                                                                                      
[VoxelSaveCompletionTracker](https://docs.godotengine.org/en/stable/classes/class_voxelsavecompletiontracker.html)  | [save_modified_blocks](#i_save_modified_blocks) ( )                                                                                                                                                                                                                          
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)                                              | [try_set_block_data](#i_try_set_block_data) ( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) position, [VoxelBuffer](VoxelBuffer.md) voxels )                                                                                                
[Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)                                      | [voxel_to_data_block](#i_voxel_to_data_block) ( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) voxel_pos ) const                                                                                                                               
<p></p>

## Signals: 

- block_loaded( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) position ) 

Emitted when a new data block is loaded from stream.

Note: it might be not visible yet.

- block_unloaded( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) position ) 

Emitted when a data block is unloaded due to being outside view distance.

## Property Descriptions

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_area_edit_notification_enabled"></span> **area_edit_notification_enabled** = false


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_automatic_loading_enabled"></span> **automatic_loading_enabled** = true


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_block_enter_notification_enabled"></span> **block_enter_notification_enabled** = false


- [AABB](https://docs.godotengine.org/en/stable/classes/class_aabb.html)<span id="i_bounds"></span> **bounds** = AABB(-5.36871e+08, -5.36871e+08, -5.36871e+08, 1.07374e+09, 1.07374e+09, 1.07374e+09)

Defines the bounds within which the terrain is allowed to have voxels. If an infinite world generator is used, blocks will only generate within this region. Everything outside will be left empty.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_collision_layer"></span> **collision_layer** = 1


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_collision_margin"></span> **collision_margin** = 0.04


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_collision_mask"></span> **collision_mask** = 1


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_generate_collisions"></span> **generate_collisions** = true

Enables the generation of collision shapes using the classic physics engine. Use this feature if you need realistic or non-trivial collisions or physics.

Note 1: you also need [VoxelViewer](VoxelViewer.md) to request collisions, otherwise they won't generate.

Note 2: If you need simple Minecraft/AABB physics, you can use [VoxelBoxMover](VoxelBoxMover.md) which may perform better in blocky worlds.

- [Material](https://docs.godotengine.org/en/stable/classes/class_material.html)<span id="i_material_override"></span> **material_override**


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_max_view_distance"></span> **max_view_distance** = 128

Sets the maximum distance this terrain can support. If a [VoxelViewer](VoxelViewer.md) requests more, it will be clamped.

Note: there is an internal limit of 512 for constant LOD terrains, because going further can affect performance and memory very badly at the moment.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_mesh_block_size"></span> **mesh_block_size** = 16


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_run_stream_in_editor"></span> **run_stream_in_editor** = true

Makes the terrain appear in the editor.

Important: this option will turn off automatically if you setup a script world generator. Modifying scripts while they are in use by threads causes undefined behaviors. You can still turn on this option if you need a preview, but it is strongly advised to turn it back off and wait until all generation has finished before you edit the script again.

## Method Descriptions

- [void](#)<span id="i__on_area_edited"></span> **_on_area_edited**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) area_origin, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) area_size ) 


- [void](#)<span id="i__on_data_block_entered"></span> **_on_data_block_entered**( [VoxelDataBlockEnterInfo](VoxelDataBlockEnterInfo.md) info ) 


- [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)<span id="i_data_block_to_voxel"></span> **data_block_to_voxel**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) block_pos ) 

Converts data block coordinates into voxel coordinates. Voxel coordinates of a block correspond to its lowest corner.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_data_block_size"></span> **get_data_block_size**( ) 


- [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)<span id="i_get_statistics"></span> **get_statistics**( ) 

Gets debug information about how much time is spent processing the terrain.

The returned dictionary has the following structure:

```gdscript
{
	"time_detect_required_blocks": int,
	"time_request_blocks_to_load": int,
	"time_process_load_responses": int,
	"time_request_blocks_to_update": int,
	"time_process_update_responses": int,
	"remaining_main_thread_blocks": int,
	"dropped_block_loads": int,
	"dropped_block_meshs": int,
	"updated_blocks": int
}

```

- [PackedInt32Array](https://docs.godotengine.org/en/stable/classes/class_packedint32array.html)<span id="i_get_viewer_network_peer_ids_in_area"></span> **get_viewer_network_peer_ids_in_area**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) area_origin, [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) area_size ) 


- [VoxelTool](VoxelTool.md)<span id="i_get_voxel_tool"></span> **get_voxel_tool**( ) 

Creates an instance of [VoxelTool](VoxelTool.md) bound to this node, to access voxels and edition methods.

You can keep it in a member variable to avoid creating one again, as long as the node still exists.

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_has_data_block"></span> **has_data_block**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) block_position ) 


- [void](#)<span id="i_save_block"></span> **save_block**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) position ) 

Forces a specific block to be saved.

Note 1: all modified blocks are automatically saved before the terrain is destroyed.

Note 2: this will only have an effect if the stream setup on this terrain supports saving.

Note 3: saving is asynchronous and won't block the game. the save may complete only a short time after you call this method.

- [VoxelSaveCompletionTracker](https://docs.godotengine.org/en/stable/classes/class_voxelsavecompletiontracker.html)<span id="i_save_modified_blocks"></span> **save_modified_blocks**( ) 

Forces all modified blocks to be saved.

Note 1: all modified blocks are automatically saved before the terrain is destroyed.

Note 2: this will only have an effect if the stream setup on this terrain supports saving.

Note 3: saving is asynchronous and won't block the game. the save may complete only a short time after you call this method.

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_try_set_block_data"></span> **try_set_block_data**( [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html) position, [VoxelBuffer](VoxelBuffer.md) voxels ) 


- [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)<span id="i_voxel_to_data_block"></span> **voxel_to_data_block**( [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html) voxel_pos ) 


_Generated on Mar 26, 2023_
