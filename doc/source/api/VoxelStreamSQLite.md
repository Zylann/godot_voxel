# VoxelStreamSQLite

Inherits: [VoxelStream](VoxelStream.md)


Saves voxel data into a single SQLite database file.

## Properties: 


Type      | Name                               | Default 
--------- | ---------------------------------- | --------
`String`  | [database_path](#i_database_path)  | ""      
<p></p>

## Property Descriptions

- [String](https://docs.godotengine.org/en/stable/classes/class_string.html)<span id="i_database_path"></span> **database_path** = ""

Path to the database file. `res://` and `user://` are not supported at the moment. The path can be relative to the game's executable. Directories in the path must exist. If the file does not exist, it will be created.

_Generated on Nov 06, 2021_
