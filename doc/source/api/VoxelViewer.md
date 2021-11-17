# VoxelViewer

Inherits: [Spatial](https://docs.godotengine.org/en/stable/classes/class_spatial.html)


Attach this as a child node of characters, so the voxel world will know where to load blocks around them.

If no viewer is present in the world, nothing will generate.

## Description: 

The voxel world uses the position and options of all the [VoxelViewer](VoxelViewer.md) nodes to determine where to load blocks, and prioritize updates. For example, a voxel placed 100 units away from a player will have much lower priority than the modifications that player is doing when digging in front of them.

## Properties: 


Type    | Name                                           | Default 
------- | ---------------------------------------------- | --------
`bool`  | [requires_collisions](#i_requires_collisions)  | true    
`bool`  | [requires_visuals](#i_requires_visuals)        | true    
`int`   | [view_distance](#i_view_distance)              | 128     
<p></p>

## Property Descriptions

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_requires_collisions"></span> **requires_collisions** = true

If set to `true`, the engine will generate classic collision shapes around this viewer.

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_requires_visuals"></span> **requires_visuals** = true

If set to `true`, the engine will generate meshes around this viewer. This may be enabled for the local player.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_view_distance"></span> **view_distance** = 128

How far should voxels generate around this viewer.

_Generated on Nov 06, 2021_
