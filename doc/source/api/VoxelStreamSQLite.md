# VoxelStreamSQLite

Inherits: [VoxelStream](VoxelStream.md)

Saves voxel data into a single SQLite database file.

## Properties: 


Type                                                                        | Name                                                           | Default                          
--------------------------------------------------------------------------- | -------------------------------------------------------------- | ---------------------------------
[String](https://docs.godotengine.org/en/stable/classes/class_string.html)  | [database_path](#i_database_path)                              | ""                               
[CoordinateFormat](VoxelStreamSQLite.md#enumerations)                       | [preferred_coordinate_format](#i_preferred_coordinate_format)  | COORDINATE_FORMAT_STRING_CSD (2) 
<p></p>

## Methods: 


Return                                                                  | Signature                                                                                                                             
----------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)  | [is_key_cache_enabled](#i_is_key_cache_enabled) ( ) const                                                                             
[void](#)                                                               | [set_key_cache_enabled](#i_set_key_cache_enabled) ( [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled )  
<p></p>

## Enumerations: 

enum **CoordinateFormat**: 

- <span id="i_COORDINATE_FORMAT_INT64_X16_Y16_Z16_L16"></span>**COORDINATE_FORMAT_INT64_X16_Y16_Z16_L16** = **0** --- Coordinates are stored in a 64-bit integer key, where X, Y, Z and LOD are 16-bit signed integers.
- <span id="i_COORDINATE_FORMAT_INT64_X19_Y19_Z19_L7"></span>**COORDINATE_FORMAT_INT64_X19_Y19_Z19_L7** = **1** --- Coordinates are stored in a 64-bit integer key, where X, Y and Z are 19-bit signed integers, and LOD is a 7-bit unsigned integer.
- <span id="i_COORDINATE_FORMAT_STRING_CSD"></span>**COORDINATE_FORMAT_STRING_CSD** = **2** --- Coordinates are stored in strings of comma-separated base 10 numbers "X,Y,Z,LOD".
- <span id="i_COORDINATE_FORMAT_BLOB80_X25_Y25_Z25_L5"></span>**COORDINATE_FORMAT_BLOB80_X25_Y25_Z25_L5** = **3** --- Coordinates are stored in 80-bit blobs, where X, Y and Z are 25-bit signed integers and LOD is a 5-bit unsigned integer.
- <span id="i_COORDINATE_FORMAT_COUNT"></span>**COORDINATE_FORMAT_COUNT** = **4**


## Property Descriptions

### [String](https://docs.godotengine.org/en/stable/classes/class_string.html)<span id="i_database_path"></span> **database_path** = ""

Path to the database file. `res://` and `user://` should work, however `res://` will not work after export (see [ why here](https://docs.godotengine.org/en/stable/tutorials/io/data_paths.html#accessing-persistent-user-data-user)). The path can be relative to the game's executable. Directories in the path must exist. If the file does not exist, it will be created.

### [CoordinateFormat](VoxelStreamSQLite.md#enumerations)<span id="i_preferred_coordinate_format"></span> **preferred_coordinate_format** = COORDINATE_FORMAT_STRING_CSD (2)

Sets which block coordinate format will be used when creating new databases. This affects the range of supported coordinates and how quickly SQLite can execute queries (to a minor extent). When opening existing databases, this setting will be ignored, and the format of the database will be used instead. Changing the format of an existing database is currently not possible, and may require using a script to load individual blocks from one stream and save them to a new one.

## Method Descriptions

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_is_key_cache_enabled"></span> **is_key_cache_enabled**( ) 

*(This method has no documentation)*

### [void](#)<span id="i_set_key_cache_enabled"></span> **set_key_cache_enabled**( [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled ) 

Enables caching keys of the database to speed up loading queries in terrains that only save sparse edited blocks. This won't provide any benefit if your terrain saves all its blocks (for example if the output of the generator is saved).

This must be called before any call to `load_voxel_block` (before the terrain starts using it), otherwise it won't work properly. You may use a script to do this.

_Generated on Aug 09, 2025_
