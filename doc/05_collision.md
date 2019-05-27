# Collision

Physics based collision is enabled by default.

You can turn it on or off by setting the `generate_collision` option on any of the terrain nodes. Or you can enable or disable it in code.

The collision is built along with the mesh. So any blocks that have already been built will not be affected by the setting unless they are regenerated.

There are some alternative collision related tools available:

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

## Raycasting (Blocky or Smooth)

You can manually send out raycasts from your character to see how far away from the terrain it is. You can also do this with any of your objects.

_Note: Raycasting in VoxelLodTerrain requires the merging of [PR #29](https://github.com/Zylann/godot_voxel/pull/29), which is a work in progress. It works fine on LOD 0, but not the others._

Here's an example of how it can work. For a character 2 units tall, this runs a raycast directly down to prevent the character from sinking into the ground. `on_ground` is a boolean that tells the jump function whether it can fire or not. 
```
# Apply gravity to downward velocity
velocity.y += delta*GRAVITY
# But clamp it if we hit the ground
if terrain.raycast(head_position, Vector3(0,-1,0), PLAYER_HEIGHT):
	velocity.y = clamp(velocity.y, 0, 999999999)
	on_ground = true 
```

Then for lateral motion, you can test with something like this. We run a raycast at foot/knee level, then a little higher up (head level, since the granularity provided by the raycast is not very fine). 

If it's clear at head level, but not at foot level then we're looking at a sloped terrain. We move the character up a little bit to help it traverse the terrain. It is a little rough, but serves as a working example. 
```
func test_and_move(pos, dir) -> Vector3:
	# If raycast hits at knee level (-1.5)
	if terrain.raycast(Vector3(pos.x, pos.y-1.5, pos.z), dir, 1):
	
		# Then test at head level and move character up a little if clear
		if !terrain.raycast(pos, dir, 1) :
			translate(Vector3(0,.15,0))
			return dir
		
		# Both hit, can't move	
		return Vector3(0,0,0)
	
	# Otherwise, free to move	
	else:
		return dir

```

---
* [Next Page](06_custom-streams.md)
* [Doc Index](01_get-started.md)
