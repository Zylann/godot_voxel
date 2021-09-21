Thread count configuration
----------------------------

This module makes heavy use of threads to speed up heavy operations and avoid stalls.

Depending on how many threads your CPU can run at the same time, the optimal number of threads can vary. This may also differ for players running your game.
The module automatically determines the number of threads to use at runtime, based on how many concurrent threads the CPU supports.

You can change how many threads are allocated in your Project Settings, in the `Voxel` section. The automatic calculation will be based on the following properties:

Parameter name                              | Type    | Description
--------------------------------------------|---------|-----------------------------------------------------------------
`voxel/threads/count/minimum`               | `int`   | Minimum amount of threads
`voxel/threads/count/margin_below_maximum`  | `int`   | How many threads below max concurrent count should be considered maximum. `0` means the maximum concurrent count will be the maximum. `1` means the maximum concurrent count minus 1 will be the maximum.
`voxel/threads/count/ratio_over_maximum`    | `float` | Portion of max concurrent threads to attempt using, between 0 and 1. For example, `0.5` will attempt to use half of them. The result will be clamped using the other options.

Several notes:

- It is recommended to not use all available threads for voxel stuff. Games use more for other things, and players may even do something else in background (such as music, YouTube playlist or voice chat).
- The module uses one additional streaming thread for file I/O, which is always reserved, and will count in the amount of threads allocated.
- It is not possible to use zero threads. The module is designed to use threads at the moment.
- You can check at runtime how many theads are allocated with a script and using `VoxelServer.get_stats()`. It is also printed if `debug/settings/stdout/verbose_stdout` is enabled in project settings (or `-v` in command line).
- Changing these settings requires an editor restart (or game restart) to take effect.


Access to voxels
-----------------

This section explains in more detail how multithreading is implemented with voxel storage, and what are the implications when you access and modify voxels.


### The problem

Up to version `godot3.2.3` of the module, reading and writing to voxels did not care about multithreading. It was possible to access them without locking, because all the threaded operations using them (saving and meshing) were given copies of the voxels, made on the main thread.

This made things simple, however it causes several issues.
- If threads are unable to consume tasks faster than they are issued, copies of voxel data will keep accumulating rapidly and make the game run out of memory.
- Copying blocks and their neighbors takes time and is potentially wasteful because it's not guaranteed to be used.
- It assumes the threaded task will only need to access a specific block at a fixed LOD, which is not always the case in other voxel engines (such as UE4 Voxel Plugin by Phyronnaz). For example, Transvoxel running on a big block may attempt to access higher-resolution blocks to better approximate the isosurface, which is not possible with the current approach.


### Internal changes

The old design starts to change in version `godot3.2.4`. Now, copies aren't made preemptively on the main thread anymore, and are done in the actual threaded task instead. This means accessing voxels now require to lock the data during each transaction, to make sure each thread gets consistent data.
Locking is required **if you access voxels which are part of a multithreaded volume**, like a terrain present in the scene tree. You don't need to if you know the data is not used by any other thread, like inside generators, custom streams, known copies or other storage not owned by an active component of the voxel engine.

The locking strategy is implemented on each `VoxelBuffer`, using `RWLock`. Such locks are read-write-locks, also known as shared mutexes. As described earlier, it is optional, so *none of VoxelBuffer methods actually use that lock*, it's up to you. If you only need to read voxels, lock for *read*. If you also need to modify voxels, lock for *write*. Multiple threads can then read the same block, but only one can modify it at once. If a thread wants to modify the block while it is already locked for *read*, the thread will be blocked until all other threads finished reading it. This can cause stutter if done too often on the main thread, so if it becomes a problem, a possible solution is to lock for *read*, copy the block and then modify it (Copy-on-Write).

At time of writing, there are no threaded tasks needing a write access to voxels.
It is possible that more changes happen in the future, in particular with nodes supporting LOD.


### Editing voxels efficiently

This matters for scripters.

If you use `VoxelTool`, all locking mechanisms are handled for you automatically. However, you must be aware that it doesn't come for free: if you want to access voxels randomly and modify them randomly, you will pretty much get the worst overhead. If you want to access a well-defined region and you know where to read, and where to write, then optimizing becomes possible.

For example, *on a terrain node*, `VoxelTool.get_voxel` or `set_voxel` are the simplest, yet the slowest way to modify voxels. This is not only because of locking, but also because the engine has to go all the way through several data structures to access the voxel. This is perfectly fine for small isolated edits, like the player digging or building piece by piece. 

If you want to excavate whole chunks or generating structures, try to use specialized bulk functions instead, such as `do_sphere()`, `do_box()`, `raycast` or `paste()`. These will be more efficient because they can cache data structures on the way and perform locking in the best way they can.

If your changes depend on a lot of pre-existing voxels, you can use `copy()` to extract a chunk of voxels into a `VoxelBuffer` so you can read them very fast without locking. You can even choose to do your changes on that same buffer, and finally use `paste()` when you're done.
