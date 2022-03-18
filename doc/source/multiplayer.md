Multiplayer
=============

!!! warn
    Multiplayer is currently in experimental phase. It is possible to some extent, but may have issues to be improved and might even change completely in the future. Features mentionned here are also only available in the `master` branch.

!!! note
    This page assumes you already have knowledge in general multiplayer programming. It is strongly recommended you learn it beforehand. You can have a look at [Godot's documentation page about networking](https://docs.godotengine.org/en/stable/tutorials/networking/index.html).

In general, the plan is to expose APIs to implement a client/server architecture. There is no assumption of the networking library you use, so it could be Godot RPCs, Valve Networking Sockets or even TCP. There is however assumption that the server also runs a version of the game.

Other points to explore:

- Block caching and versionning: save blocks client-side so the server doesn't have to send them again next time if they didn't change
- Client-requesting alternative model: having the client actively request blocks with custom code instead of passively receiving them from the server


Server-side viewer with `VoxelTerrain`
--------------------------------------

There are some support functions you can use that can help putting together primitive multiplayer. The idea is for the server to be authoritative, and the client just receives information from it.

`VoxelTerrain` has a `Networking` category in the inspector. These properties are not necessarily specific to multiplayer, but were actually added to experiment with it, so they are grouped together.

Client and server will need a different setup.

### On the server

- Configure `VoxelTerrain` as normal, with a generator and maybe a stream.
- On `VoxelTerrain`, Enable `block_enter_notification_enabled`
- Add a script to `VoxelTerrain` implementing `func _on_data_block_entered(info)`. This function will be called each time a new voxel block enters a remote player's area. This will be a place where you may send the block to the client. You can use `VoxelBlockSerializer` to pack voxel data into bytes. The `info.are_voxels_edited()` boolean can tell if the block was ever edited: if it wasn't, you can avoid sending the whole data and just tell the client to generate the block locally.
- When a player joins, make sure a `VoxelViewer` is created for it, assign its `network_peer_id` and enable `requires_data_block_notifications`. This will make the terrain load blocks around it and notify when blocks need to be sent to the peer.
- On `VoxelTerrain`, enable `area_edit_notification_enabled`
- In your `VoxelTerrain` script, implement `func _on_area_edited(origin, size)`. This function will be called each time voxels are edited within a bounding box. Voxels inside may have to be sent to all players close enough. You can get a list of network peer IDs by calling `get_viewer_network_peer_ids_in_area(origin, size)`.

### On the client

- Configure `VoxelTerrain` with a mesher and maybe a generator, and turn off `automatic_loading_enabled`. Voxels will only load based on what the server sends.
- Add a script handling network messages. When a block is received from the server, store it inside `VoxelTerrain` by using the `try_set_block_data` function.
- When a box of edited voxels is received from the server, you may use a `VoxelTool` and the `paste` function to replace the edited voxels. If you want the client to generate the block locally, you could use the generator to make one with `generate_block_async()`. If you use asynchronous generation, note that blocks written with `try_set_block_data` will cancel blocks that are loading. That means if a client receives an edited block in the meantime, the generating block won't overwrite it.
- The client will still need a `VoxelViewer`, which will allow the terrain to detect when it can unload voxel data (the server does not send that information). To reduce the likelihood of "holes" in the terrain if blocks get unloaded too soon, you may give the `VoxelViewer` a larger view distance than the server.
- The client can have remote players synchronized so the player can see them, but you should not add a `VoxelViewer` to them (only the server does). The client should not have to stream terrain for remote players, it only has one for the local player.


With `VoxelLodTerrain`
------------------------

There is no support for now, but it is planned.


Protocol notes
---------------

RPCs in Godot use UDP (reliable or unreliable), so sending large amounts of voxels to clients could have limited speed. Instead, it would be an option to use TCP to send blocks instead, as well as large edits. Small edits or deterministic edits with ligthweight info could keep using reliable UDP. Problem: you would have to use two ports, one for UDP, one for TCP. So maybe it is a better idea to keep using reliable UDP.

Note: Minecraft's network protocol is entirely built on top of TCP.

