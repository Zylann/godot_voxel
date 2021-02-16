Blocky terrains
=====================

This page focuses more in detail on blocky terrains, Minecraft-like, or made of cubes.


`VoxelMesherBlocky`
---------------------

### Creating voxel types

TODO

### Texturing

TODO


`VoxelMesherCubes`
------------------

TODO


Fast collisions alternative
------------------------------

### Move and slide

Mesh-based collisions are quite accurate and feature-rich in Godot, however it has some drawbacks:

- Trimesh collision shapes have to be built each time the terrain is modified, which is [very slow](https://github.com/Zylann/godot_voxel/issues/54).
- The physics engine has to process arbitrary triangles near the player, which can't take advantage of particular situations, such as everything being cubes
- Sometimes you may also want a simpler, more game-oriented collision system

The `VoxelBoxMover` class provides a Minecraft-like collision system, which can be used in a similar way to `move_and_slide()`. It is more limited, but is extremely fast and is not affected by tunnelling.

The code below shows how to use it, but see the [blocky demo](https://github.com/Zylann/voxelgame/tree/master/project/blocky_terrain) for the full code.

```gdscript
var box_mover = VoxelBoxMover.new()
var character_box  = AABB(Vector3(-0.4, -0.9, -0.4), Vector3(0.8, 1.8, 0.8))
var terrain = get_node("VoxelTerrain")

func _physics_process(delta):
	# ... Input commands that set velocity go here ...

    # Apply terrain collision
	var motion : Vector3 = velocity * delta
	motion = box_mover.get_motion(get_translation(), motion, character_box, terrain)
	global_translate(motion)
	velocity = motion / delta
```

!!! note
	this technique mainly works if you use `VoxelMesherBlocky`, because it gets information about which block is collidable from the `VoxelLibrary` used with it. It might have some limited support in other meshers though.

### Raycast

An alternative raycast function exists as well, which returns voxel-specific results. It may be useful if you turned off classic collisions as well. This is accessible with the `VoxelTool` class. An instance of it bound to the terrain can be obtained with `get_voxel_tool()`.

```gdscript
var terrain : VoxelTerrain = get_node("VoxelTerrain")
var vt : VoxelTool = terrain.get_voxel_tool()
var hit = vt.raycast(origin, direction, 10)

if hit != null:
    # The returned position is in voxel coordinates,
    # and can be used to access the value of the voxel with other functions of `VoxelTool`
    print("Hit voxel ", hit.position)
```

