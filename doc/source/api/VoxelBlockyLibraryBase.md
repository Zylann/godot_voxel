# VoxelBlockyLibraryBase

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)

Inherited by: [VoxelBlockyLibrary](VoxelBlockyLibrary.md), [VoxelBlockyTypeLibrary](VoxelBlockyTypeLibrary.md)

Contains a list of models that can be used by [VoxelMesherBlocky](VoxelMesherBlocky.md).

## Description: 

Models used by [VoxelMesherBlocky](VoxelMesherBlocky.md) must be baked before they can be used efficiently at runtime. The way this process happens depends on the implementation of this class. It can be a simple list of models, or a list of high-level types generating variant models. Check child classes for more information.

## Properties: 


Type                                                                      | Name                                                   | Default
------------------------------------------------------------------------- | ------------------------------------------------------ | --------------------------
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [rounding_amount](#i_rounding_amount)                  | 0.1
[float](https://docs.godotengine.org/en/stable/classes/class_float.html)  | [rounding_taper_distance](#i_rounding_taper_distance)  | 0.4
[RoundingTaperProfile](VoxelBlockyLibraryBase.md#enumerations)            | [rounding_taper_profile](#i_rounding_taper_profile)    | ROUNDING_TAPER_LINEAR (0)
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)    | [bake_tangents](#i_bake_tangents)                      | true
<p></p>

## Methods: 


Return                                                                              | Signature                                   
----------------------------------------------------------------------------------- | --------------------------------------------
[void](#)                                                                           | [bake](#i_bake) ( )                         
[Material[]](https://docs.godotengine.org/en/stable/classes/class_material[].html)  | [get_materials](#i_get_materials) ( ) const 
<p></p>

## Enumerations:

enum **RoundingTaperProfile**:

- <span id="i_ROUNDING_TAPER_LINEAR"></span>**ROUNDING_TAPER_LINEAR** = **0** --- Linear transition: t. Here t is normalized distance through the taper.
- <span id="i_ROUNDING_TAPER_EASE_IN"></span>**ROUNDING_TAPER_EASE_IN** = **1** --- Quadratic ease-in transition: t squared. Here t is normalized distance through the taper.
- <span id="i_ROUNDING_TAPER_EASE_OUT"></span>**ROUNDING_TAPER_EASE_OUT** = **2** --- Quadratic ease-out transition: 1 minus (1 minus t) squared. Here t is normalized distance through the taper.
- <span id="i_ROUNDING_TAPER_SMOOTHSTEP"></span>**ROUNDING_TAPER_SMOOTHSTEP** = **3** --- Smoothstep transition: t squared times (3 minus 2 times t). Here t is normalized distance through the taper.


## Constants: 

- <span id="i_MAX_MODELS"></span>**MAX_MODELS** = **65536**
- <span id="i_MAX_MATERIALS"></span>**MAX_MATERIALS** = **65536**

## Property Descriptions

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_rounding_amount"></span> **rounding_amount** = 0.1

Nominal bevel radius shared by enabled models in this library, in voxel units, clamped to 0 through 0.25. The local radius is reduced near exposed three-edge corners and tapered contacts. Values below 0.001 are treated as zero to keep generated triangles stable at floating-point mesh precision. Zero disables rounding geometry. Only models with [VoxelBlockyModel.rounding_enabled](VoxelBlockyModel.md#i_rounding_enabled) enabled and supported cube geometry are affected. Bake the library and remesh affected terrain after changing this at runtime.

### [float](https://docs.godotengine.org/en/stable/classes/class_float.html)<span id="i_rounding_taper_distance"></span> **rounding_taper_distance** = 0.4

Distance in voxel units used by each transition from a square contact toward the nominal bevel radius, clamped to 0.01 through 1.0. Transitions from opposite ends multiply where they overlap, so an edge may not reach the nominal radius. Values 0.4, 0.6, 0.8 and 1.0 mean 40%, 60%, 80% and 100% of one voxel edge. This setting is independent of the bevel radius. Bake the library and remesh affected terrain after changing this at runtime.

### [RoundingTaperProfile](VoxelBlockyLibraryBase.md#enumerations)<span id="i_rounding_taper_profile"></span> **rounding_taper_profile** = ROUNDING_TAPER_LINEAR (0)

Shape of the edge transition over [rounding_taper_distance](VoxelBlockyLibraryBase.md#i_rounding_taper_distance), independent of [rounding_amount](VoxelBlockyLibraryBase.md#i_rounding_amount). Bake the library and remesh affected terrain after changing this at runtime.

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_bake_tangents"></span> **bake_tangents** = true

Enable this option if you need normal mapping on your voxels. If you don't need it, disabling can reduce memory usage and give a small speed boost.

## Method Descriptions

### [void](#)<span id="i_bake"></span> **bake**( ) 

Bakes the library. The data of models is optimized in order to combine them more efficiently when generating voxel meshes.

### [Material[]](https://docs.godotengine.org/en/stable/classes/class_material[].html)<span id="i_get_materials"></span> **get_materials**( ) 

Gets a list of all distinct materials found in all models of the library.

Note, if at least one non-empty model has no material, there will be one `null` entry in this list to represent "The default material".

_Generated on Nov 07, 2026_
