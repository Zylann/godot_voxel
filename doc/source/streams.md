Streams
========

`VoxelStream` allows to save and load voxel data to a file or a directory structure, using various kinds of implementations. They don't hold voxel data in memory, they are just an access point to different file formats.


Stream types
----------------

A few different types are available, each with slightly different features.

- [VoxelStreamSQLite](api/VoxelStreamSQLite.md) is the most featured one, and uses a single SQLite database file. It can save both voxel data and [instancing](instancing.md) data.
- [VoxelStreamRegionFiles](api/VoxelStreamRegionFiles.md) is an older one, which works similarly to Minecraft's region system. It saves under multiple files in a folder. It only supports voxel data.
- [VoxelStreamScript](api/VoxelStreamScript.md) is a custom stream that may be implemented using a script. See [Scripting](scripting.md#custom-stream).

There is currently no stream implementation using an existing file format (like `.vox` for example), mainly because the current API expects the ability to load data in chunks compatible with the engine's format.


Using streams for savegames
----------------------------

Streams were created initially to serve as a database for saves. Games using voxel technology for terrain are often persistent, so there has to be a place on disk where to save the changes, and reload them back.

The engine can handle near-unlimited terrain size, so there are often situations where loading the entire world in memory is not possible. For that reason, streams are built in such a way they provide data block by block (or "chunks").
Only blocks within the player's view distance will be loaded in memory. When the player moves, blocks far away will get unloaded/saved, while blocks getting closer will be loaded.

By default, only modified blocks are saved. However, if the generator you use is too expensive to re-run on demand, or if your world is primarily edited, it can be configured such that every new block will always get saved. This is the case in Minecraft.

When voxels are edited, modified blocks are not saved immediately. This is because many edits could keep happening, and trying to save too often could be wasteful.

Saving only occurs under the following conditions:

- The block gets unloaded when too far away
- [save_modified_blocks()](api/VoxelTerrain.md#i_save_modified_blocks) is called on the terrain node (you may want to call this when the player saves, on a timer, or when quitting the game)

You can add minimal saving with this script:

```
extends VoxelTerrain

func _ready():
    stream = VoxelStreamSQLite.new()
    stream.database_path = "path/to/save.file" # Note, the directory must exist

func _on_tree_exited():
    save_modified_blocks()
```

It can get more complex as development progresses. See following sections for details.

See also this demo game, which includes one save: [https://github.com/Zylann/voxelgame/tree/master/project/blocky_game](https://github.com/Zylann/voxelgame/tree/master/project/blocky_game)

TODO: Demo handling multiple saves


Asynchronous saving
---------------------

While streams have synchronous save/load methods and don't actually depend on terrains, terrains use them *asynchronously*. Loading is performed on different threads, similarly to how procedural generation works in chunks. Saving is also asynchronous, and doesn't block the main thread so the game will not stutter.

However, this approach has consequences you usually don't encounter in more "classic" scene loading/saving, and need to be accounted for.

### Knowing when saving is complete

If you save and quit a "world" for example, saving will remain happening in the background for a little while, even after you destroy the terrain node. The file won't contain the changes for a moment. To handle this, you may wait for all tasks to finish, maybe displaying a waiting screen in the meantime.

Check the documentation of [save_modified_blocks()](api/VoxelTerrain.md#i_save_modified_blocks) for more details.

### Switching saves

Another caveat is that changes you do to a stream resource at runtime can be potentially mistaken. For example, let's say you play on a terrain, you save, and want to load another terrain. You could change the path property of your stream to point to another save file/directory, and start a new session. But because saving is asynchronous, saving/loading tasks could still be pending in different threads while you do this. That can cause saves meant for the previous file to end up in the next one.

A simple solution to avoid this, is to create a different stream instance, and let the old one finish off as all its asynchronous tasks complete. This guarantees that a new session can't possibly be bothered by asynchronous tasks of the previous.

You should also consider *NOT embedding a stream resource inside your scene (like `world.tscn`)*. If saves are dynamically created in game, this is not a good approach, because even if you destroy the nodes from that scene and then re-instantiate it for another session, Godot keeps the resources cached, including the stream, which means you'll end up modifying the same stream as the previous session. So creating and assigning a new stream at runtime is a better option.

### File locking

Streams remain open, in order to continuously save and load blocks of terrain. Closing and re-opening to save each block would be much slower due to system API calls, and some platforms have to do a lot of filesystem work under the hood.

The consequence is that save files will often be locked while they are in use. Notably, they can't be deleted.

First, you could wait when saves complete before taking action, or just expect that your file(s) will be locked for a little while after leaving a "world".

Second, make sure your stream actually closes. Some approaches are:

- 1) Ideally, just let Godot's resource system destroy the stream *once nothing in the game references it*. This is another reason why you should preferably not embed a stream resource in your scenes, because even if there are no instances of that scene in the tree, a variable containing the [PackedScene](https://docs.godotengine.org/en/latest/classes/class_packedscene.html) still references that resource, preventing it from being unloaded. Once all references are gone from your side, the only ones left will be eventual background tasks, that should complete soon after.
- 2) Manually modify properties of the stream to make it "close". For example, with [VoxelStreamSQLite](api/VoxelStreamSQLite.md), you can set `database_path` to an empty string (`""`), which will force it to close. However, this will cause pending saves and loads to fail, so you should make sure to do that when saving has completed first (TODO: at the moment, [loads will cause errors too](https://github.com/Zylann/godot_voxel/issues/620#issuecomment-2040255061) but there is no way to "wait" for them. This might be addressed in the future if you want to use method 2, but for now try using method 1)


Using streams in the Godot Editor
----------------------------------

### Overlap between editor and game

At the moment, streams can run in the editor, but they behave the same as if the game was running. If you modify anything, blocks will eventually get saved under the same conditions as seen earlier. If you want to preserve your game saves, either leave the `stream` property unassigned, or you can assign a "development save" on the stream in the editor. Then, assign a different path from within your game to the real save (using script).

If you use the same save files between game and editor, there is a risk of conflict when you run the game: it will try to open files which are already opened and locked by the editor. To workaround this, either use different files, or close the scene before running the game. See [issue 283](https://github.com/Zylann/godot_voxel/issues/283).


### Closing the game

When you test your game and expect proper saving, prefer closing it normally, instead of using Godot's `Stop` button:

![Screenshot of the Stop button in the Godot Editor](images/godot_editor_stop_button.webp)

This button will [kill the game's process](https://github.com/godotengine/godot/blob/b4e2a24c1f62088b3f7ce0197afc90832fc25009/editor/editor_run.cpp#L358), without leaving a chance for cleanup (SIGKILL on Linux). That means any pending save tasks will be lost, and caches won't be flushed. If files are in the middle of being written, it can also cause them to get corrupted.


Save format specifications
----------------------------

- [Region format](specs/region_format_v3.md)
- [Block format](specs/block_format_v2.md)
- [SQLite format](specs/sqlite_format.md)
