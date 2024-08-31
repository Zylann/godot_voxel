# VoxelModifier

Inherits: [Node3D](https://docs.godotengine.org/en/stable/classes/class_node3d.html)

Inherited by: [VoxelModifierMesh](VoxelModifierMesh.md), [VoxelModifierSphere](VoxelModifierSphere.md)

Base class for voxel modifiers.

## Description: 

Modifiers are meant to be as an extension to terrain's generator that non-destructively affect limited volume. Stacks with other modifiers. Runtime edits from [VoxelTool](VoxelTool.md) override modifier values.

Note 1: Only works with [VoxelLodTerrain](VoxelLodTerrain.md).

Note 2: Only works with smooth terrain (SDF).

## Properties: 


Type                                                                      | Name                         | Default 
------------------------------------------------------------------------- | ---------------------------- | --------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)      | [operation](#i_operation)    | 0       
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [smoothness](#i_smoothness)  | 0.0     
<p></p>

## Enumerations: 

enum **Operation**: 

- <span id="i_OPERATION_ADD"></span>**OPERATION_ADD** = **0** --- Performs SDF union.
- <span id="i_OPERATION_REMOVE"></span>**OPERATION_REMOVE** = **1** --- Performs SDF subtraction.


## Property Descriptions

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_operation"></span> **operation** = 0

An operation that the modifier performs on the terrain or on the other modifiers.

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_smoothness"></span> **smoothness** = 0.0

Increasing this value makes the shape "merge" with its surroundings across a more or less large distance. 

Note, it assumes the base generator produces consistent gradients. This is not always the case. Notably, it is a common optimization for generators to avoid calculating gradients beyond a certain distance from surfaces. If smoothness is too large, or if the generator's cutoff distance is too low, it can lead to gaps in the resulting mesh, usually at chunk boundaries.

_Generated on Aug 27, 2024_
