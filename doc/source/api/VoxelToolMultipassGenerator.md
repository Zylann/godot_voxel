# VoxelToolMultipassGenerator

Inherits: [VoxelTool](VoxelTool.md)

Provided to edit voxels in the context of multipass terrain generation.

## Description: 

This tool allows to edit voxels within a 3D box, centered on a specific area corresponding to the current chunk or column to generate.

Depending on context, it is also possible to edit voxels some distance away from the main area.

The difference between "main" and "total" areas, is that the "main" area is where you should generate stuff, while the "total area" is only available in case what you generate needs to overlap outside of the main area.


Instances of this class are temporary and not thread-safe. They must never be re-used or stored in a member variable.

## Methods: 


Return                                                                          | Signature                                                   
------------------------------------------------------------------------------- | ------------------------------------------------------------
[Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)  | [get_editable_area_max](#i_get_editable_area_max) ( ) const 
[Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)  | [get_editable_area_min](#i_get_editable_area_min) ( ) const 
[Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)  | [get_main_area_max](#i_get_main_area_max) ( ) const         
[Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)  | [get_main_area_min](#i_get_main_area_min) ( ) const         
<p></p>

## Method Descriptions

- [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)<span id="i_get_editable_area_max"></span> **get_editable_area_max**( ) 

Gets the lower corner of the total editable area, in voxels.

- [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)<span id="i_get_editable_area_min"></span> **get_editable_area_min**( ) 

Gets the lower corner of the main editable area, in voxels.

- [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)<span id="i_get_main_area_max"></span> **get_main_area_max**( ) 

Gets the upper corner of the total editable area, in voxels.

- [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)<span id="i_get_main_area_min"></span> **get_main_area_min**( ) 

Gets the upper corner of the main editable area, in voxels.

_Generated on Oct 15, 2023_
