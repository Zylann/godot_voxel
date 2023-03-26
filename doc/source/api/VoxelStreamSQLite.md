# VoxelStreamSQLite

Inherits: [VoxelStream](VoxelStream.md)


Saves voxel data into a single SQLite database file.

## Properties: 


Type      | Name                               | Default 
--------- | ---------------------------------- | --------
`String`  | [database_path](#i_database_path)  | ""      
<p></p>

## Methods: 


Return                                                                  | Signature                                                                                                                             
----------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)  | [is_key_cache_enabled](#i_is_key_cache_enabled) ( ) const                                                                             
[void](#)                                                               | [set_key_cache_enabled](#i_set_key_cache_enabled) ( [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled )  
<p></p>

## Property Descriptions

- [String](https://docs.godotengine.org/en/stable/classes/class_string.html)<span id="i_database_path"></span> **database_path** = ""

Path to the database file. `res://` and `user://` are not supported at the moment. The path can be relative to the game's executable. Directories in the path must exist. If the file does not exist, it will be created.

## Method Descriptions

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_is_key_cache_enabled"></span> **is_key_cache_enabled**( ) 


- [void](#)<span id="i_set_key_cache_enabled"></span> **set_key_cache_enabled**( [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) enabled ) 


_Generated on Mar 26, 2023_
