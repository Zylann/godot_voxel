# Collision

Physics based collision is enabled by default. It provides both raycasting and collision detection.

You can turn it on or off by setting the `generate_collisions` option on any of the terrain nodes. Or you can enable or disable it in code.

The collision is built along with the mesh. So any blocks that have already been built will not be affected by the setting unless they are regenerated.


## Debugging

You can also turn on the collision wire mesh for debugging. In the editor, look under the Debug menu for `Visible Collision Shapes`.

<img src="images/debug-collision-shapes.gif" />



# Alternatives To Physics Based Collision

Though physics based collision is recommended, there might be times when alternative methods are desired. 

## Axis Aligned Bounding Box (Blocky only)

VoxelBoxMover has a function that can test AABB collision. The code below shows how to use it, but see the [blocky demo](https://github.com/Zylann/voxelgame/tree/master/project/blocky_terrain) for the full code.

Example:
```
var 	box_mover = VoxelBoxMover.new()
var 	aabb      = AABB(Vector3(-0.4, -0.9, -0.4), Vector3(0.8, 1.8, 0.8))
var 	terrain   = get_node("VoxelTerrain")

func _physics_process(delta):

	/* Input commands that set velocity go here. */

	var motion = velocity * delta
	motion = box_mover.get_motion(get_translation(), motion, aabb, terrain)
	global_translate(motion)
	velocity = motion / delta
```

## Raycasting 

You may want to use raycasts when collision has not been enabled. For instance picking the terrain with the mouse cursor. `VoxelTerrain` supports a rudimentary raycast. `VoxelLODTerrain` does not.

Returned values are blocky, integer grid positions, while physics based raycasting will return fine-grained floating point positions.

The usage is simple: `terrain.raycast(start_postion, direction, max_distance)`

Upon success, it returns a dictionary with the following values:

```
hit["position"] - Vector3 integer position where the ray hit 
hit["prev_position"] - Previous Vector3 integer position the ray occupied before it hit
```

On failure, it will return an empty dictionary:`Variant()`. You can logically test this result:

```
if terrain.raycast(...): 
	do something
```


---
* [Next Page](06_custom-streams.md)
* [Doc Index](01_get-started.md)
