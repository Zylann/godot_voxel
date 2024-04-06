# VoxelMesherCubes

Inherits: [VoxelMesher](VoxelMesher.md)

## Properties: 


Type                                                                            | Name                                                 | Default 
------------------------------------------------------------------------------- | ---------------------------------------------------- | --------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)            | [color_mode](#i_color_mode)                          | 0       
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)          | [greedy_meshing_enabled](#i_greedy_meshing_enabled)  | true    
[Material](https://docs.godotengine.org/en/stable/classes/class_material.html)  | [opaque_material](#i_opaque_material)                |         
[VoxelColorPalette](VoxelColorPalette.md)                                       | [palette](#i_palette)                                |         
[Material](https://docs.godotengine.org/en/stable/classes/class_material.html)  | [transparent_material](#i_transparent_material)      |         
<p></p>

## Methods: 


Return                                                                  | Signature                                                                                                                                                                                                                              
----------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)  | [generate_mesh_from_image](#i_generate_mesh_from_image) ( [Image](https://docs.godotengine.org/en/stable/classes/class_image.html) image, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) voxel_size ) static 
[void](#)                                                               | [set_material_by_index](#i_set_material_by_index) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) id, [Material](https://docs.godotengine.org/en/stable/classes/class_material.html) material )                 
<p></p>

## Enumerations: 

enum **Materials**: 

- <span id="i_MATERIAL_OPAQUE"></span>**MATERIAL_OPAQUE** = **0**
- <span id="i_MATERIAL_TRANSPARENT"></span>**MATERIAL_TRANSPARENT** = **1**
- <span id="i_MATERIAL_COUNT"></span>**MATERIAL_COUNT** = **2**

enum **ColorMode**: 

- <span id="i_COLOR_RAW"></span>**COLOR_RAW** = **0**
- <span id="i_COLOR_MESHER_PALETTE"></span>**COLOR_MESHER_PALETTE** = **1**
- <span id="i_COLOR_SHADER_PALETTE"></span>**COLOR_SHADER_PALETTE** = **2**


## Property Descriptions

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_color_mode"></span> **color_mode** = 0

*(This property has no documentation)*

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_greedy_meshing_enabled"></span> **greedy_meshing_enabled** = true

*(This property has no documentation)*

### [Material](https://docs.godotengine.org/en/stable/classes/class_material.html)<span id="i_opaque_material"></span> **opaque_material**

*(This property has no documentation)*

### [VoxelColorPalette](VoxelColorPalette.md)<span id="i_palette"></span> **palette**

*(This property has no documentation)*

### [Material](https://docs.godotengine.org/en/stable/classes/class_material.html)<span id="i_transparent_material"></span> **transparent_material**

*(This property has no documentation)*

## Method Descriptions

### [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_generate_mesh_from_image"></span> **generate_mesh_from_image**( [Image](https://docs.godotengine.org/en/stable/classes/class_image.html) image, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) voxel_size ) 

*(This method has no documentation)*

### [void](#)<span id="i_set_material_by_index"></span> **set_material_by_index**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) id, [Material](https://docs.godotengine.org/en/stable/classes/class_material.html) material ) 

*(This method has no documentation)*

_Generated on Apr 06, 2024_
