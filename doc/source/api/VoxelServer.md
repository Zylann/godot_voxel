# VoxelServer

Inherits: [Object](https://docs.godotengine.org/en/stable/classes/class_object.html)


Singleton handling common voxel processing in background threads.

## Methods: 


Return                                                                              | Signature                      
----------------------------------------------------------------------------------- | -------------------------------
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)  | [get_stats](#i_get_stats) ( )  
<p></p>

## Method Descriptions

- [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)<span id="i_get_stats"></span> **get_stats**( ) 

Gets debug information about shared voxel processing.

The returned dictionary has the following structure:

```gdscript
{
	"thread_pools": {
		"streaming": {
			"tasks": int,
			"active_threads": int,
			"thread_count": int
		},
		"general": {
			"tasks": int,
			"active_threads": int,
			"thread_count": int
		}
	},
	"tasks": {
		"streaming": int,
		"meshing": int,
		"generation": int
	},
	"memory_pools": {
		"voxel_used": int,
		"voxel_total": int
	}
}

```

_Generated on Nov 06, 2021_
