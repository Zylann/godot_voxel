# VoxelEngine

Inherits: [Object](https://docs.godotengine.org/en/stable/classes/class_object.html)


Singleton holding common settings and handling voxel processing tasks in background threads.

## Methods: 


Return                                                                              | Signature                           
----------------------------------------------------------------------------------- | ------------------------------------
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)  | [get_stats](#i_get_stats) ( ) const 
<p></p>

## Method Descriptions

- [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)<span id="i_get_stats"></span> **get_stats**( ) 

Gets debug information about shared voxel processing.

The returned dictionary has the following structure:

```gdscript
{
	"thread_pools": {
		"general": {
			"tasks": int,
			"active_threads": int,
			"thread_count": int
		}
	},
	"tasks": {
		"streaming": int,
		"meshing": int,
		"generation": int,
		"main_thread": int
	},
	"memory_pools": {
		"voxel_used": int,
		"voxel_total": int,
		"block_count": int
	}
}

```

_Generated on Mar 26, 2023_
