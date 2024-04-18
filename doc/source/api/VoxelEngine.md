# VoxelEngine

Inherits: [Object](https://docs.godotengine.org/en/stable/classes/class_object.html)

Singleton holding common settings and handling voxel processing tasks in background threads.

## Methods: 


Return                                                                              | Signature                                           
----------------------------------------------------------------------------------- | ----------------------------------------------------
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)  | [get_stats](#i_get_stats) ( ) const                 
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [get_version_major](#i_get_version_major) ( ) const 
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [get_version_minor](#i_get_version_minor) ( ) const 
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [get_version_patch](#i_get_version_patch) ( ) const 
<p></p>

## Method Descriptions

### [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)<span id="i_get_stats"></span> **get_stats**( ) 

Gets debug information about shared voxel processing.

The returned dictionary has the following structure:

```
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
		"block_count": int,
		"std_allocated": int,
		"std_deallocated": int,
		"std_current": int
	}
}
```

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_version_major"></span> **get_version_major**( ) 

Gets the major version number of the voxel engine. For example, in `1.2.0`, `1` is the major version.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_version_minor"></span> **get_version_minor**( ) 

Gets the minor version number of the voxel engine. For example, in `1.2.0`, `2` is the minor version.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_version_patch"></span> **get_version_patch**( ) 

Gets the patch version number of the voxel engine. For example, in `1.2.0`, `0` is the patch version.

_Generated on Apr 06, 2024_
