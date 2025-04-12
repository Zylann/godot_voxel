# VoxelBlockyModelFluid

Inherits: [VoxelBlockyModel](VoxelBlockyModel.md)

Model representing a specific state of a fluid.

## Properties: 


Type                                                                  | Name               | Default 
--------------------------------------------------------------------- | ------------------ | --------
[VoxelBlockyFluid](VoxelBlockyFluid.md)                               | [fluid](#i_fluid)  |         
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [level](#i_level)  | 0       
<p></p>

## Constants: 

- <span id="i_MAX_LEVELS"></span>**MAX_LEVELS** = **256** --- Maximum amount of supported fluid levels.

## Property Descriptions

### [VoxelBlockyFluid](VoxelBlockyFluid.md)<span id="i_fluid"></span> **fluid**

Which fluid this model is part of. Note, fluid resources are supposed to be shared between multiple models, in order to make those models recognized as states of that fluid.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_level"></span> **level** = 0

Fluid level, usually representing how much fluid the model contains. Levels should start from 0, and must be lower than 256. Fluids can have multiple models with the same level. It is also preferable to define at least one model per level (avoid missing levels). It is also recommended to assign models with consecutive levels to consecutive library IDs, however this is not required.

_Generated on Mar 23, 2025_
