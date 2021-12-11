# VoxelMesherBlocky

Inherits: [VoxelMesher](VoxelMesher.md)


Produces a mesh by batching models corresponding to each voxel value, similar to games like Minecraft or StarMade.

## Description: 

Occluded faces are removed from the result, and some degree of ambient occlusion can be baked on the edges. Values are expected to be in the constant VoxelBuffer.CHANNEL_TYPE channel. Models are defined with a [VoxelLibrary](VoxelLibrary.md), in which model indices correspond to the voxel values. Models don't have to be cubes.

## Properties: 


Type            | Name                                         | Default 
--------------- | -------------------------------------------- | --------
`VoxelLibrary`  | [library](#i_library)                        |         
`float`         | [occlusion_darkness](#i_occlusion_darkness)  | 0.8     
`bool`          | [occlusion_enabled](#i_occlusion_enabled)    | true    
<p></p>

## Property Descriptions

- [VoxelLibrary](VoxelLibrary.md)<span id="i_library"></span> **library**


- [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_occlusion_darkness"></span> **occlusion_darkness** = 0.8


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_occlusion_enabled"></span> **occlusion_enabled** = true


_Generated on Nov 06, 2021_
