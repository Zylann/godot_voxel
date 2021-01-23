Access to voxels and multithreading
======================================

This section explains in more detail how multithreading is implemented with voxel storage, and what are the implications when you access and modify voxels.


The problem
-------------

Up to version `godot3.2.3` of the module, reading and writing to voxels did not care about multithreading. It was possible to access them without locking, because all the threaded operations using them (saving and meshing) were given copies of the voxels, made on the main thread.

This made things simple, however it causes several issues.
- If threads are unable to consume tasks faster than they are issued, copies of voxel data will keep accumulating rapidly and make the game run out of memory.
- Copying blocks and their neighbors takes time and is potentially wasteful because it's not guaranteed to be used.
- It assumes the threaded task will only need to access a specific block at a fixed LOD, which is not always the case in other voxel engines (such as UE4 Voxel Plugin by Phyronnaz). For example, Transvoxel running on a big block may attempt to access higher-resolution blocks to better approximate the isosurface, which is not possible with the current approach.


Internal changes
-----------------

The old design starts to change in version `godot3.2.4`. Now, copies aren't made preemptively on the main thread anymore, and are done in the actual threaded task instead. This means accessing voxels now require to lock the data during each transaction, to make sure each thread gets consistent data.
Locking is required **if you access voxels which are part of a multithreaded volume**, like a terrain present in the scene tree. You don't need to if you know the data is not used by any other thread, like inside generators, custom streams, known copies or other storage not owned by an active component of the voxel engine.

The locking strategy is implemented on each `VoxelBuffer`, using `RWLock`. Such locks are read-write-locks, also known as shared mutexes. As described earlier, it is optional, so *none of VoxelBuffer methods actually use that lock*, it's up to you. If you only need to read voxels, lock for *read*. If you also need to modify voxels, lock for *write*. Multiple threads can then read the same block, but only one can modify it at once. If a thread wants to modify the block while it is already locked for *read*, the thread will be blocked until all other threads finished reading it. This can cause stutter if done too often on the main thread, so if it becomes a problem, a possible solution is to lock for *read*, copy the block and then modify it (Copy-on-Write).

At time of writing, there are no threaded tasks needing a write access to voxels.
It is possible that more changes happen in the future, in particular with nodes supporting LOD.


Editing voxels efficiently
---------------------------

This matters for scripters.

If you use `VoxelTool`, all locking mechanisms are handled for you automatically. However, you must be aware that it doesn't come for free: if you want to access voxels randomly and modify them randomly, you will pretty much get the worst overhead. If you want to access a well-defined region and you know where to read, and where to write, then optimizing becomes possible.

For example, *on a terrain node*, `VoxelTool.get_voxel` or `set_voxel` are the simplest, yet the slowest way to modify voxels. This is not only because of locking, but also because the engine has to go all the way through several data structures to access the voxel. This is perfectly fine for small isolated edits, like the player digging or building piece by piece. 

If you want to excavate whole chunks or generating structures, try to use specialized bulk functions instead, such as `do_sphere()`, `do_box()`, `raycast` or `paste()`. These will be more efficient because they can cache data structures on the way and perform locking in the best way they can.

If your changes depend on a lot of pre-existing voxels, you can use `copy()` to extract a chunk of voxels into a `VoxelBuffer` so you can read them very fast without locking. You can even choose to do your changes on that same buffer, and finally use `paste()` when you're done.
