# VoxelBlockyFluid

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)

Properties of a blocky fluid.

## Description: 

Common properties of a specific fluid. It may be shared between multiple blocky models, each representing a level/state of the fluid.

## Properties: 


Type                                                                            | Name                                               | Default 
------------------------------------------------------------------------------- | -------------------------------------------------- | --------
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)          | [dip_when_flowing_down](#i_dip_when_flowing_down)  | false   
[Material](https://docs.godotengine.org/en/stable/classes/class_material.html)  | [material](#i_material)                            |         
<p></p>

## Property Descriptions

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_dip_when_flowing_down"></span> **dip_when_flowing_down** = false

When enabled, if all following conditions are met for a given fluid voxel:

- It doesn't have maximum level

- It isn't covered by another voxel of the same fluid type 

- It can flow downwards (below is air or fluid of the same type)

Then the shape of the voxel will change to be "pushed" downwards, creating steeper slopes. Note, this also means the voxel will look as if it has minimum level in some situations. However, in practice these cases don't occur often. You may decide whether to use this option depending on how your fluid simulates.

### [Material](https://docs.godotengine.org/en/stable/classes/class_material.html)<span id="i_material"></span> **material**

Material used by all states of the fluid. Note that UVs of a fluid are different than a regular model, so you may need a [ShaderMaterial](https://docs.godotengine.org/en/stable/classes/class_shadermaterial.html) to handle flowing animation. See [https://voxel-tools.readthedocs.io/en/latest/blocky_terrain/#fluids](https://voxel-tools.readthedocs.io/en/latest/blocky_terrain/#fluids)

_Generated on Aug 09, 2025_
