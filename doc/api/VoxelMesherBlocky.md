# Class: VoxelMesherBlocky

Inherits: VoxelMesher

_Godot version: 3.2_


## Class Description: 

Produces a mesh by batching models corresponding to each voxel value, similar to games like Minecraft or StarMade. Occluded faces are removed from the result, and some degree of ambient occlusion can be baked on the edges. Values are expected to be in the [constant VoxelBuffer.CHANNEL_TYPE] channel. Models are defined with a `VoxelLibrary`, in which model indices correspond to the voxel values. Models don't have to be cubes.


## Online Tutorials: 



## Constants:


## Properties:


## Methods:

#### » VoxelLibrary get_library (  )  const


#### » float get_occlusion_darkness (  )  const


#### » bool get_occlusion_enabled (  )  const


#### » void set_library ( VoxelLibrary voxel_library ) 


#### » void set_occlusion_darkness ( float value ) 


#### » void set_occlusion_enabled ( bool enable ) 



## Signals:


---
* [Class List](Class_List.md)
* [Doc Index](../01_get-started.md)

_Generated on Aug 10, 2020_
