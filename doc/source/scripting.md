Scripting
=============

This page shows some examples in how to use the scripting API.


Editing the terrain
----------------------

### Using [VoxelTool](api/VoxelTool.md)

`VoxelTool` is a simplified API to access and modify voxel data. It is possible to obtain one from any class storing voxels, using the `get_voxel_tool()` function. That function will return a `VoxelTool` tied to the volume you got it from.

See [VoxelTool](api/VoxelTool.md) for available functions. Note, depending on which class you get it from, subclasses of `VoxelTool` may have more specialized functions.

It is possible to store a reference to `VoxelTool` in a member variable, in case you want to access voxels from the same volume many times. It is more efficient, because every call to `get_voxel_tool()` creates a new instance of it.

Before you start modifying voxels, make sure you access the right channel.

```gdscript
# If you use VoxelMesherBlocky
voxel_tool.channel = VoxelBuffer.CHANNEL_TYPE
```

```gdscript
# If you use VoxelMesherTransvoxel
voxel_tool.channel = VoxelBuffer.CHANNEL_SDF
```

```gdscript
# If you use VoxelMesherCubes
voxel_tool.channel = VoxelBuffer.CHANNEL_COLOR
```

### Boundary limitation

When a terrain is streaming blocks in and out, it is not possible to edit past loaded borders. Either you will get an error, or nothing will happen.
You can test if the area you want to access or edit is available by calling `VoxelTool.is_area_editable()`.


### LOD limitation

Similarly to bounds limitation, when you use LOD with `VoxelLodTerrain`, it is not possible to access or edit voxels beyond the first LOD level. Past this level, voxel data is no longer available at full resolution.


### Editing performance

In general, editing voxels one by one is the slowest. It is ok for actually getting only a few, but if you plan to modify larger areas at once, you may prefer functions that do this in bulk, or copy/paste buffers.

See [Access to voxels and multithreading](performance.md)


Custom generator
------------------

You can provide your own voxel generator by extending `VoxelGeneratorScript` in either GDScript, C# or C++.

!!! note
    custom generators can also be created without scripts, using [VoxelGeneratorGraph](generators.md)

### Example

Here is how to make a bare bones generator usable with a blocky terrain. Make sure you use `VoxelMesherBlocky` as mesher.

Create a standalone script `my_generator.gd` with the following contents:

```gdscript
extends VoxelGeneratorScript

const channel : int = VoxelBuffer.CHANNEL_TYPE

func _get_used_channels_mask() -> int:
    return 1 << channel
 
func _generate_block(buffer : VoxelBuffer, origin : Vector3i, lod : int) -> void:
	if lod != 0:
        return
	if origin.y < 0:
        buffer.fill(1, channel)
	if origin.x == origin.z and origin.y < 1:
        buffer.fill(1, channel)
```

In your terrain scene, add another script to a node, which will setup your generator when the game starts. Code might differ a bit depending on how you structure your scene.

```gdscript
const MyGenerator = preload("my_generator.gd")

# Get the terrain
var terrain = $VoxelTerrain

func _ready():
	terrain.generator = MyGenerator.new()
```

Make sure to have a `VoxelViewer` node in the scene under the camera, and you should see this:

![Custom stream](images/custom-stream.jpg)

Though `VoxelBuffer.fill()` is probably not what you want to use, the above is a quick example. Generate_block generally gives you a block of 16x16x16 cubes to fill all at once, so you may also use `VoxelBuffer.set_voxel()` to specify each one individually. You can change the channel to `VoxelBuffer.CHANNEL_SDF` to get smooth voxels using another mesher such as `VoxelMesherTransvoxel`.


### Thread-safety

Generators are invoked from multiple threads. Make sure your code is thread-safe.

If your generator uses resources or exports parameters that you want to change while it might be running, you should make sure they are read-only or copied per thread, so if the resource is modified from outside or another thread it won't disrupt the generator.

You can use `Mutex` to enforce single-thread access to variables, but use it with caution because otherwise you could end up limiting performance to one thread (while the other waits for the lock to be released). Using Read-Write locks and thread-locals are good options, unfortunately the Godot script API does not provide this.

Careful about lazy-initialization, it can cause crashes if two threads run it at the same time. `Curve` is one of the resources doing that: if you call `interpolate_baked()` and it wasn't baked yet, it will be baked at the very last moment. Here is an example of working around this:

```gdscript
extends VoxelGeneratorScript

const MountainsCurve : Curve = preload("moutains_curve.tres")

# This is called when the generator is created
func _init():
    # Call `bake()` to be sure it doesn't happen later inside `generate_block()`.
    MountainsCurve.bake()

# ...
```

A similar story occurs with `Image`. It needs to be locked before you can access pixels, but calling `lock()` and `unlock()` itself is not thread-safe. One approach to solve this is to `lock()` the image in `_init()` and leave it locked for the whole lifetime of the generator. This assumes of course that the image is never accessed from outside:

```gdscript
extends VoxelGeneratorScript

var image : Image

# This is called when the generator is created
func _init():
    image = Image.new()
    image.load("some_heightmap.png")
    image.lock()

func generate_block(buffer : VoxelBuffer, origin : Vector3i, lod : int) -> void:
    # ... use image.get_pixel() freely ...
    # ... but DO NOT use image.set_pixel() ...

func _notification(what: int):
    if what == NOTIFICATION_PREDELETE:
        # Called when the script is destroyed.
        # I don't know if it's really required, but unlock for correctness.
        image.unlock()

# ...
```

Image.lock() won't be required anymore in Godot 4.


Handling block boundaries with voxel structures
---------------------------------------------------

In Minecraft-style terrain, a very common problem that arises once base terrain is generated, is how to plant trees in it, because such structures would be voxels too.
In that specific case, there are a number of caveats when doing that with this engine, which mainly revolve around the following limitation:

When generating blocks of 16x16x16 voxels, you cannot access voxels outside of this area.
The engine generates blocks using multiple threads in a single pass, so accessing neighbors would severely affect performance, could break determinism, and accessing terrain directly would be unsafe.

The following describes a method which does not involve accessing neighbors at all, and allows to generate trees in a terrain where the base height is deterministic (2D noise heightmap for example).

### Deterministic approach

#### Finding where trees should grow in (X, Z)

The first thing to do is to figure out, in the current block, where should trees grow. For simplicity, we will consider it as a 2D problem, where trees can grow at specific (X, Z) positions (since Y is up). For that, we need to find positions of voxels just above ground, and do so in a deterministic manner, so that the same seed will produce the same results.

But how to make it deterministic? We can use the seed of a `RandomNumberGenerator` instance. But if we give it the seed of the world, every block will have trees at the same position in them. What we really need, is a seed that is unique per block. We can achieve that by using a hash of the 2D coordinates of the block:

```gdscript
var block_position := origin_in_voxels >> 4 # floored division by 16
var rng := RandomNumberGenerator.new()
rng.seed = global_seed + hash(Vector2i(block_position.x, block_position.z))
```

And now we can generate how many trees are in the block, and where:

```gdscript
var block_size := out_buffer.get_size()
var tree_count := rng.randi_range(0, 2)
var tree_positions := []
for i in tree_count:
    var tree_pos := Vector3i(
        rng.randi_range(0, block_size.x), 0, # We leave Y for later
        rng.randi_range(0, block_size.z))
    # Note, those positions are local to the block
    tree_positions[i] = tree_pos
```

#### Finding the altitude (Y)

But we still need to calculate the altitude from which the tree will grow (the Y coordinate). One issue is that the engine generates cubic blocks, so when generating a given 16x16x16 voxel, you can't access what's below, you only know what's inside the area of the block.

However, if base terrain is generated using 2D heightmap noise, then we can calculate how high terrain is at any (x, z) coordinate by computing the height function again, wherever we need.
Assuming we already have such function as `func get_height(x: float, y: float) -> float`, we can complete the Y coordinate like so:

```gdscript
for i in len(tree_positions):
    var tree_pos_local : Vector3i = tree_positions[i]
    # Use world coordinates for this
    var tree_pos_global := tree_pos_local + origin_in_voxels
    tree_pos_global.y := get_height(tree_pos_global.x, tree_pos_global.z)
    # And bring back to local
    tree_pos_local = tree_pos_global - origin_in_voxels
    # And store back into the array
    tree_positions[i] = tree_pos_local
```

#### Placing the tree

Now we should be able to place the tree, but what if positions we found are outside the block? We can't set voxels at these positions.

What we can do, is to first determine how big the tree will be. Once we know its bounding box, we can place voxels, but only those intersecting our block.

To determine how big the tree is, it sounds like we have to generate the tree first, and then determine its bounding box in voxels. We can do that in a separate blank buffer with large enough size, or using a `Dictionary` of `Vector3i` keys and `int` values. But at the end, it is preferable to store the result in an optimized `VoxelBuffer` of the right size.

We won't describe how to generate the tree itself here, it's not really the point of this article and can be different with plenty of factors. But it could just be a vertical bar of trunk voxels, with a sphere of leaves on top.
It is possible to optimize this step by pre-generating (or making by hand) a bunch of trees ahead of time and store them in a list, so all tree bounds are known and no need to spend time generating them in detail.

We can pack tree data into a class:

```gdscript
class TreeInfo:
    # Position of the tree relative to our current block
    var instance_position := Vector3i()
    # Buffer storing only the tree, like a model, so it can later be pasted in the world
    var voxels : VoxelBuffer
    # Position of the base of the tree, within the VoxelBuffer containing the model of the tree
    var trunk_base_position := Vector3i()
```

So we can have a list of trees instead of just their positions:

```gdscript
var trees : Array[TreeInfo] = []
for tree_pos in tree_positions:
    var tree : TreeInfo = generate_tree(rng)
    tree.position = tree_pos
    trees.append(tree)
```

We may wrap this logic in a function `func generate_trees_for_block(block_position: Vector3i) -> Array[TreeInfo]`, because it may be useful later.

Once we know the bounds of each tree, we can check if they intersect with the current block. If they do, we can use the `paste_masked` method to plant just the tree, without replacing solid voxels with empty ones from the tree's `VoxelBuffer`:

```gdscript
# AABB of our current block, in local coordinates
var block_aabb := AABB(Vector3(), block_size.get_size() + Vector3i(1, 1, 1))

var voxel_tool := out_buffer.get_voxel_tool()

# Paste intersecting trees
for tree in trees:
    var lower_corner_pos := tree.instance_position - tree.trunk_base_position
    var tree_aabb := AABB(lower_corner_pos, tree.voxels.get_size() + Vector3(1,1,1))
    
    if tree_aabb.intersects(block_aabb):
        voxel_tool.paste_masked(lower_corner_pos, tree.voxels, 
            # Which channel we want to paste
            1 << VoxelBuffer.CHANNEL_TYPE,
            # Masking 0, since 0 is considered air
            VoxelBuffer.CHANNEL_TYPE, 0)
```

#### Fixing overlaps

Now trees should appear in the world, but when they overlap block borders, they will be cutoff. The reason is that each block is unaware of its neighbors, they generate only trees that originate inside them in the X and Z axes, and only affect voxels in themselves, since they can't modify their neighbors.

![Schema of individual blocks generating cutoff trees](images/tree_generation_cutoff_schema.webp)

We could decide to clamp their position so that they never overlap, but that might not be acceptable, given how "aligned" they will look in the game.

We can workaround this by applying the same reasoning we did to obtain their altitude. Instead of just considering trees in the current block, we can also check trees that would generate in neighbor blocks, since we can re-run the function to get them deterministically from a given block position. Then all we have to do is keep only those intersecting our block. Each block will then generate with neighbor trees in the right locations.

![Schema of neighbor trees being generated to take into account overlaps with the current block](images/tree_generation_neighbor_fix_schema.webp)

Note that it means each block will recalculate the locations of its own trees and neighbor trees, so trees in a given block will be calculated more than once during generation of the world. That also means `generate_tree` will be called more than once too. But if we cache generated tree models ahead of time (before the game starts), this process will be a lot cheaper.

```gdscript
var trees : Array[TreeInfo] = []

# Get trees that originate from the current block and its neighbors
for nz in range(-1, 2):
    for nx in range(-1, 2):
        var trees_in_block := generate_trees_for_block(block_position + Vector3i(nx, 0, nz))
        trees.append_array(trees_in_block)

# Paste intersecting trees
for tree in trees:
    # Earlier code for pasting trees
    # ...
```

This method has been implemented [in this demo](https://github.com/Zylann/voxelgame/blob/2fa552abfdf52c688bbec27edd676018a31373e0/project/blocky_game/generator/generator.gd#L144), although the code is a bit different.

This approach is also used in Voronoi noise (also known as cellular noise in FastNoiseLite) to produce seamless cells.

#### Limitations

Of course this method has its limitations: if our terrain is more than just a heightmap, includes floating islands, complex carvings or 3D noise structures, it can make the process of finding altitude more complicated. At worse, generating neighbor columns of voxels or entire blocks would become necessary just to find the highest voxel, which would make it too slow.

To counter this, we could maybe cache generated blocks...

### Caching approach

There is another approach to this problem, which can also be implemented with today's API limitations. But as we will see, it is actually more complex than it looks because of threading problems:

The idea is, if generating a block requires to know its neighbors, then a generator can just generate them as well, at least only what is required.
Here, we will consider a case where each block can affect its neighbors in a 1-block radius, but depending on the case that dependency can extend further in each axis.

#### Implementation

!!! warning: code in this article hasn't been tested and might not work as-is. It is only here to give an idea of what would be involved.

A generator can have a member variable containing partially-generated blocks:

```gdscript
class GeneratingBlock:
    var voxels : VoxelBuffer

    func _init():
        voxels = VoxelBuffer.new()
        voxels.create(16, 16, 16)

# [Vector3i] => GeneratingBlock
# Stores blocks that have been partially generated but not directly requested.
# These blocks have not finished generating, so they are not yet in the terrain.
# When a block is fully generated, we can remove it from here since we will not need it again.
var _generating_blocks := {}
# Since VoxelGenerators are invoked from multiple threads,
# we have to protect member variables against concurrent access.
var _generating_blocks_mutex := Mutex.new()
```

As you can see, as soon as we consider actual access to neighbors, we also have to deal with multi-threading, so mutexes become necessary.

When a generator is asked to generate a given block, it can look into `_generating_blocks` first if that block has already started generating, due to neighbors themselves generating in different threads. Indeed, if a block can affect its neighbors when it generates, then the opposite can happen too. Keep this in mind!

Then we will also check each neighbor we want to access. For each position missing a block, we will partially generate them (only base ground for example) and store them in `_generating_blocks`.

Now, once we have the current block and its neighbors, we can generate base terrain AND trees for our current block AND its neighbors. The important part is that the current block will then contain ALL trees that intersect with it. Neighbors will not have them all because they won't have neighbor's neighbor trees (that's why we consider them partial).
The code would have to take care of different local offsets when pasting trees since each block has a different origin in the world.

Once we are done, if the current block was in `_generating_blocks` at the start, we can remove it, since there should be nothing else affecting it. We will leave the 8 neighbors partially generated, if any, for other threads to start from in case they are requested.

```gdscript
var current_block_position := origin_in_voxels >> 4 # floored division by 16

_generating_blocks_mutex.lock()

# [Vector3i] => GeneratingBlock
# Subset of blocks in the world, the central one being the current one
var blocks := {}

for nz in range(-1, 2):
    for nx in range(-1, 2):
        var block_position := current_block_position + Vector3i(nx, 0, nz)
        if _generating_blocks.has(block_position):
            blocks[block_position] = _generating_blocks[block_position]
        else:
            var block = GeneratingBlock.new()
            generate_base_ground(block)
            _generating_blocks[block_position] = block
            _blocks[block_position] = block

for block_position in blocks:
    var block = blocks[block_position]
    # Generate trees using some function, which also takes the map of the subset of blocks,
    # since it may be able to modify more than one block if trees overlap.
    # Following this process, the central block will have trees originating from it,
    # but also trees originating from ALL its neighbors.
    generate_trees(block, blocks)

var central_block : GeneratingBlock = _generating_blocks[current_block_position]
_generating_blocks.erase(current_block_position)

_generating_blocks_mutex.unlock()

out_voxels.copy_from(central_block.voxels)
```

#### Caveats

The catch is, for all this to be done consistently, *we have to keep generating blocks locked with `Mutex` for the whole duration of the current block's generation*. If we don't do this, other threads could start messing with the same data and bad things could happen.
This is what makes this method potentially very slow.

In the code shown earlier, `_generating_blocks_mutex` is naively locked for the entire generation process, which will slow down generation down to one thread, while other threads will be stuck waiting.

Generating 1 block requires partially generating 8 others. We could consider locking `_generating_blocks` and indivdual `GeneratingBlock` separately, since threads could be working on sets of blocks that are not touching each other. 
But in the case they touch each other, each thread will want to lock 8 mutexes. But this can cause deadlocks, where two threads wait each other indefinitely.

Let's say 2 threads want both to lock block A and B at different times:

- Thread 1 locks block A
- Thread 2 locks block B
- Thread 1 locks block B: already locked, so it has to wait
- Thread 2 locks block A: already locked, so it has to wait
- Now both threads are stuck waiting forever.

On top of this, if the dependency distance needs to be larger than 1 block, it can very quickly become a lot more expensive. 1-block dependency requires to check 26 neighbors. 2-block dependency requires 124 neighbors. 3-block dependency requires 342 neighbors...

### Multi-pass generation

Multi-pass generation is another approach loosely inspired by the previously described "caching" method, but aiming to solve its shortcomings directly in the engine, and make the task simpler for the user.

Ideas are the following:

- Split world generation into multiple passes, each with their own dependency ranges, in which case accessing neighbor voxels from a previous pass would become possible;
- Instead of waiting for their dependencies, threads would be allowed to pause their task and continue them later, allowing to do other work in the meantime.
- Store partially-generated blocks in such a way that the user API is simplified, and eventually allowing to save partially-generated blocks so they don't have to be recomputed again next time the game is started
- Use a spatial lock instead of individual mutexes, to avoid deadlocks

However, it isn't available in the engine at the moment:

- It is complex to implement properly. In order to be efficient and avoid too many block lookups, the way the world streams has to account for a "pyramid" of dependencies between passes, and threads have to properly synchronize generating blocks without getting stuck.
- In some cases, an entire column of blocks is required (like Minecraft chunks), which implies there is a limit in height. The engine allows infinite height and uses cubic blocks, so that would require limits and changes just for this.
- It requires to break compatibility, because more features would have to be injected into generators to make them work in passes.
- It would only work efficiently with a specific terrain type. It is data-heavy. `VoxelLodTerrain` wouldn't work with it, so any design changes must not impact games that don't need it, otherwise it would bloat the engine.

This may be investigated in the future, as this kind of problem also occurs with other features such as light baking (which is also specific to this kind of terrain, unfortunately). At time of writing, some prerequisites have been implemented (task postponing and spatial lock), but it will take time until it gets properly integrated to the engine.



Custom stream
---------------

Making a custom stream works similarly to a custom generator.

You have to extend the class `VoxelStreamScript` and override the methods `_load_block` and `_save_block`.
See 

TODO Script example of a custom stream


