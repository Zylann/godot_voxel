# VoxelInstanceLibraryItem

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)

Inherited by: [VoxelInstanceLibraryMultiMeshItem](VoxelInstanceLibraryMultiMeshItem.md), [VoxelInstanceLibrarySceneItem](VoxelInstanceLibrarySceneItem.md)

Settings for a model that can be used by [VoxelInstancer](VoxelInstancer.md)

## Properties: 


Type                                                                        | Name                         | Default 
--------------------------------------------------------------------------- | ---------------------------- | --------
[VoxelInstanceGenerator](VoxelInstanceGenerator.md)                         | [generator](#i_generator)    |         
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)        | [lod_index](#i_lod_index)    | 0       
[String](https://docs.godotengine.org/en/stable/classes/class_string.html)  | [name](#i_name)              | ""      
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)      | [persistent](#i_persistent)  | false   
<p></p>

## Property Descriptions

### [VoxelInstanceGenerator](VoxelInstanceGenerator.md)<span id="i_generator"></span> **generator**

Generator that will be used to pick points where the item will spawn.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_lod_index"></span> **lod_index** = 0

LOD index of chunks of the terrain where this item will spawn. The higher it is, the broader the range it will spawn at around viewers, however it will have lower precision because it uses meshes of a lower level of detail. Prefer spawning larger objects on higher LOD indexes (large trees, boulders), and small objects on lower LOD indexes (grass, small rocks)

### [String](https://docs.godotengine.org/en/stable/classes/class_string.html)<span id="i_name"></span> **name** = ""

*(This property has no documentation)*

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_persistent"></span> **persistent** = false

If not enabled, items will always spawn if generator conditions are met: for example, if you dig a hole, it will remove grass in the surface, but if you leave the area and come back, grass will spawn inside the hole.

If enabled, and if the terrain has a [VoxelStream](VoxelStream.md) supporting instances, this item will be saved in chunks where instances of it got modified (following the same events as voxel modifications), so if you leave and come back, instances of the item will remain in the same state as you left them. Only the transform of items is saved.

Note: saving relies on identifying the item in save files with the same number given in the [VoxelInstanceLibrary](VoxelInstanceLibrary.md). Removing the item or changing its ID can lead saves to load the item incorrectly.

See also [https://voxel-tools.readthedocs.io/en/latest/instancing/#persistence]()

_Generated on Aug 27, 2024_
