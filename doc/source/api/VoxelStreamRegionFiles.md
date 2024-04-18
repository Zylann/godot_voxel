# VoxelStreamRegionFiles

Inherits: [VoxelStream](VoxelStream.md)

Loads and saves blocks to region files indexed by world position, under a directory.

## Description: 

Loads and saves blocks to the filesystem, in multiple region files indexed by world position, under a directory. Regions pack many blocks together, so it reduces file switching and improves performance. Inspired by [Seed of Andromeda](https://www.seedofandromeda.com/blogs/1-creating-a-region-file-system-for-a-voxel-game) and Minecraft.

Region files are not thread-safe. Because of this, internal mutexing may often constrain the use by one thread only.

## Properties: 


Type                                                                        | Name                                   | Default 
--------------------------------------------------------------------------- | -------------------------------------- | --------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)        | [block_size_po2](#i_block_size_po2)    | 4       
[String](https://docs.godotengine.org/en/stable/classes/class_string.html)  | [directory](#i_directory)              | ""      
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)        | [lod_count](#i_lod_count)              | 1       
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)        | [region_size_po2](#i_region_size_po2)  | 4       
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)        | [sector_size](#i_sector_size)          | 512     
<p></p>

## Methods: 


Return                                                                        | Signature                                                                                                                              
----------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                                     | [convert_files](#i_convert_files) ( [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html) new_settings )  
[Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)  | [get_region_size](#i_get_region_size) ( ) const                                                                                        
<p></p>

## Property Descriptions

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_block_size_po2"></span> **block_size_po2** = 4

*(This property has no documentation)*

### [String](https://docs.godotengine.org/en/stable/classes/class_string.html)<span id="i_directory"></span> **directory** = ""

Directory under which the data is saved.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_lod_count"></span> **lod_count** = 1

*(This property has no documentation)*

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_region_size_po2"></span> **region_size_po2** = 4

*(This property has no documentation)*

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_sector_size"></span> **sector_size** = 512

*(This property has no documentation)*

## Method Descriptions

### [void](#)<span id="i_convert_files"></span> **convert_files**( [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html) new_settings ) 

*(This method has no documentation)*

### [Vector3](https://docs.godotengine.org/en/stable/classes/class_vector3.html)<span id="i_get_region_size"></span> **get_region_size**( ) 

*(This method has no documentation)*

_Generated on Apr 06, 2024_
