# Generate Your Own Voxel Data 
You can provide your own voxel generator by extending VoxelGenerator in either GDScript or C++.

Note: Your generator must be thread safe.

Create MyStream.gd with the following contents:

```
extends VoxelGenerator

export var channel:int = VoxelBuffer.CHANNEL_TYPE

func get_used_channels_mask () -> int:
        return 1<<channel
 
func generate_block(buffer:VoxelBuffer, origin:Vector3, lod:int) -> void:
	if lod != 0: return
	if origin.y < 0: buffer.fill(1, channel)
	if origin.x==origin.z and origin.y < 1: buffer.fill(1,channel)
```

In your terrain generation script, add this:

```
const MyStream = preload ("MyStream.gd")
var terrain = VoxelTerrain.new()

func _ready():
	terrain.stream = MyStream.new()
	terrain.view_distance = 256	
	terrain.viewer_path = "/root/Spatial/Player"    # Set this path to your player/camera
	add_child(terrain)
```

<img src="images/custom-stream.jpg" width="800" />

Though `VoxelBuffer.fill()` is probably not what you want to use, the above is a quick example. Generate_block generally gives you a block of 16x16x16 cubes to fill all at once, so you'll want to use `VoxelBuffer.set_voxel()` to specify each one. You can change the channel to `VoxelBuffer.CHANNEL_SDF` to get smooth voxels.

In the fps_demo, there is a [custom gdscript stream](https://github.com/tinmanjuggernaut/voxelgame/blob/master/project/fps_demo/scripts/MyStream.gd) that makes a sine wave. This was copied from the [C++ version](../generators/voxel_generator_waves.cpp), which runs a lot faster.

<img src="images/custom-stream-sine.jpg" width="800" />



---
* [Next Page](07_performance-tips.md)
* [Doc Index](01_get-started.md)
