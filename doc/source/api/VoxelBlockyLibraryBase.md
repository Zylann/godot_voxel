# VoxelBlockyLibraryBase

Inherits: [Resource](https://docs.godotengine.org/en/stable/classes/class_resource.html)

Inherited by: [VoxelBlockyLibrary](VoxelBlockyLibrary.md), [VoxelBlockyTypeLibrary](VoxelBlockyTypeLibrary.md)

Contains a list of models that can be used by [VoxelMesherBlocky](VoxelMesherBlocky.md).

## Description: 

Models used by [VoxelMesherBlocky](VoxelMesherBlocky.md) must be baked before they can be used efficiently at runtime. The way this process happens depends on the implementation of this class. It can be a simple list of models, or a list of high-level types generating variant models. Check child classes for more information.

## Properties: 


Type                                                                    | Name                               | Default 
----------------------------------------------------------------------- | ---------------------------------- | --------
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)  | [bake_tangents](#i_bake_tangents)  | true    
<p></p>

## Methods: 


Return                                                                              | Signature                                   
----------------------------------------------------------------------------------- | --------------------------------------------
[void](#)                                                                           | [bake](#i_bake) ( )                         
[Material[]](https://docs.godotengine.org/en/stable/classes/class_material[].html)  | [get_materials](#i_get_materials) ( ) const 
<p></p>

## Constants: 

- <span id="i_MAX_MODELS"></span>**MAX_MODELS** = **65536**
- <span id="i_MAX_MATERIALS"></span>**MAX_MATERIALS** = **65536**

## Property Descriptions

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_bake_tangents"></span> **bake_tangents** = true

Enable this option if you need normal mapping on your voxels. If you don't need it, disabling can reduce memory usage and give a small speed boost.

## Method Descriptions

### [void](#)<span id="i_bake"></span> **bake**( ) 

Bakes the library. The data of models is optimized in order to combine them more efficiently when generating voxel meshes.

### [Material[]](https://docs.godotengine.org/en/stable/classes/class_material[].html)<span id="i_get_materials"></span> **get_materials**( ) 

Gets a list of all distinct materials found in all models of the library.

Note, if at least one non-empty model has no material, there will be one `null` entry in this list to represent "The default material".

_Generated on Apr 06, 2024_
