# VoxelEngine

Inherits: [Object](https://docs.godotengine.org/en/stable/classes/class_object.html)

Singleton holding common settings and handling voxel processing tasks in background threads.

## Methods: 


Return                                                                              | Signature                                                                                                                 
----------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------
[Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html)  | [get_stats](#i_get_stats) ( ) const                                                                                       
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [get_thread_count](#i_get_thread_count) ( ) const                                                                         
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)              | [get_threaded_graphics_resource_building_enabled](#i_get_threaded_graphics_resource_building_enabled) ( ) const           
[String](https://docs.godotengine.org/en/stable/classes/class_string.html)          | [get_version_edition](#i_get_version_edition) ( ) const                                                                   
[String](https://docs.godotengine.org/en/stable/classes/class_string.html)          | [get_version_git_hash](#i_get_version_git_hash) ( ) const                                                                 
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [get_version_major](#i_get_version_major) ( ) const                                                                       
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [get_version_minor](#i_get_version_minor) ( ) const                                                                       
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                | [get_version_patch](#i_get_version_patch) ( ) const                                                                       
[String](https://docs.godotengine.org/en/stable/classes/class_string.html)          | [get_version_status](#i_get_version_status) ( ) const                                                                     
[Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)      | [get_version_v](#i_get_version_v) ( ) const                                                                               
[void](#)                                                                           | [run_tests](#i_run_tests) ( [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html) options )  
[void](#)                                                                           | [set_thread_count](#i_set_thread_count) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) count )    
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
			"thread_count": int,
			"task_names": PackedStringArray
		}
	},
	"tasks": {
		"streaming": int,
		"meshing": int,
		"generation": int,
		"main_thread": int,
		"gpu": int
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

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_thread_count"></span> **get_thread_count**( ) 

Returns the number of threads currently used internally by the `ThreadedTaskRunner`.

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_get_threaded_graphics_resource_building_enabled"></span> **get_threaded_graphics_resource_building_enabled**( ) 

Tells if the voxel engine is able to create graphics resources from different threads. This will usually be true if the current renderer's thread model is safe or multi-threaded, but might also be false if the renderer would poorly benefit from this (such as legacy OpenGL).

### [String](https://docs.godotengine.org/en/stable/classes/class_string.html)<span id="i_get_version_edition"></span> **get_version_edition**( ) 

Tells the edition of the voxel engine, which is either of the following: `module`, `extension`

### [String](https://docs.godotengine.org/en/stable/classes/class_string.html)<span id="i_get_version_git_hash"></span> **get_version_git_hash**( ) 

Gets the Git hash that was used to compile the voxel engine.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_version_major"></span> **get_version_major**( ) 

Gets the major version number of the voxel engine. For example, in `1.2.0`, `1` is the major version.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_version_minor"></span> **get_version_minor**( ) 

Gets the minor version number of the voxel engine. For example, in `1.2.0`, `2` is the minor version.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_version_patch"></span> **get_version_patch**( ) 

Gets the patch version number of the voxel engine. For example, in `1.2.0`, `0` is the patch version.

### [String](https://docs.godotengine.org/en/stable/classes/class_string.html)<span id="i_get_version_status"></span> **get_version_status**( ) 

Gets the version status, which may be one of the following: `dev`, `release`

### [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)<span id="i_get_version_v"></span> **get_version_v**( ) 

Gets the major (x), minor (y) and patch (z) version numbers of the voxel engine as a single vector. May be useful for comparisons.

### [void](#)<span id="i_run_tests"></span> **run_tests**( [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html) options ) 

Runs internal unit tests. This function is only available if the voxel engine is compiled with `voxel_tests=true`.

### [void](#)<span id="i_set_thread_count"></span> **set_thread_count**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) count ) 

Sets the number of threads to be used internally by the `ThreadedTaskRunner`. Setting this can cause lagging, and it might take some time until the number of threads actually matches the given value.

_Generated on Aug 09, 2025_
