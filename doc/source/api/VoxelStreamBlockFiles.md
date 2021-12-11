# VoxelStreamBlockFiles

Inherits: [VoxelStream](VoxelStream.md)


Loads and saves blocks as individual files under a directory.

## Description: 

Loads and saves blocks to the filesystem, under a directory. Each block gets its own file, which may produce a lot of them. This is a naive implementation and may be very slow in practice. At the very least it serves as proof of concept, but will probably be removed in the future.

## Properties: 


Type      | Name                       | Default 
--------- | -------------------------- | --------
`String`  | [directory](#i_directory)  | ""      
<p></p>

## Property Descriptions

- [String](https://docs.godotengine.org/en/stable/classes/class_string.html)<span id="i_directory"></span> **directory** = ""

Directory under which the data is saved.

_Generated on Nov 06, 2021_
