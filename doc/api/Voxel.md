# Class: Voxel

Inherits: Resource

_Godot version: 3.2_


## Class Description: 

Represents a model to be used for voxels of a specific TYPE value. Such models must be contained within a `VoxelLibrary` to be used with `VoxelTerrain` or directly with a `VoxelMesherBlocky`.

Some other various properties also exist to make it easier to implements games based on this technique (such as Minecraft).


## Online Tutorials: 



## Constants:

#### » GeometryType.GEOMETRY_NONE = 0


#### » GeometryType.GEOMETRY_CUBE = 1


#### » GeometryType.GEOMETRY_CUSTOM_MESH = 2


#### » GeometryType.GEOMETRY_MAX = 3


#### » Side.SIDE_NEGATIVE_X = 1


#### » Side.SIDE_POSITIVE_X = 0


#### » Side.SIDE_NEGATIVE_Y = 2


#### » Side.SIDE_POSITIVE_Y = 3


#### » Side.SIDE_NEGATIVE_Z = 4


#### » Side.SIDE_POSITIVE_Z = 5


#### » Side.SIDE_COUNT = 6



## Properties:

#### » Array collision_aabbs

`set_collision_aabbs (value)` setter

`get_collision_aabbs ()` getter


#### » int collision_mask

`set_collision_mask (value)` setter

`get_collision_mask ()` getter


#### » Color color

`set_color (value)` setter

`get_color ()` getter


#### » Mesh custom_mesh

`set_custom_mesh (value)` setter

`get_custom_mesh ()` getter


#### » int Voxel.GeometryType.geometry_type

`set_geometry_type (value)` setter

`get_geometry_type ()` getter


#### » int material_id

`set_material_id (value)` setter

`get_material_id ()` getter


#### » bool random_tickable

`set_random_tickable (value)` setter

`is_random_tickable ()` getter


#### » bool transparent

`set_transparent (value)` setter

`is_transparent ()` getter


#### » String voxel_name

`set_voxel_name (value)` setter

`get_voxel_name ()` getter



## Methods:

#### » int get_id (  )  const


#### » bool is_empty() (  )  const


#### » Voxel set_color ( Color color ) 


#### » Voxel set_id ( int id ) 


#### » Voxel set_material_id ( int id ) 


#### » Voxel set_transparent ( bool transparent ) 


#### » Voxel set_voxel_name ( String name ) 



## Signals:


---
* [Class List](Class_List.md)
* [Doc Index](../01_get-started.md)

_Generated on Aug 10, 2020_
