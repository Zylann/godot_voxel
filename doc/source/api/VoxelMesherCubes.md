# VoxelMesherCubes

Inherits: [VoxelMesher](VoxelMesher.md)



## Properties: 


Type                 | Name                                                 | Default 
-------------------- | ---------------------------------------------------- | --------
`int`                | [color_mode](#i_color_mode)                          | 0       
`bool`               | [greedy_meshing_enabled](#i_greedy_meshing_enabled)  | true    
`Material`           | [opaque_material](#i_opaque_material)                |         
`VoxelColorPalette`  | [palette](#i_palette)                                |         
`Material`           | [transparent_material](#i_transparent_material)      |         
<p></p>

## Methods: 


Return                                                                  | Signature                                                                                                                                                                                                                              
----------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)  | [generate_mesh_from_image](#i_generate_mesh_from_image) ( [Image](https://docs.godotengine.org/en/stable/classes/class_image.html) image, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) voxel_size ) static 
[void](#)                                                               | [set_material_by_index](#i_set_material_by_index) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) id, [Material](https://docs.godotengine.org/en/stable/classes/class_material.html) material )                 
<p></p>

## Enumerations: 

enum **Materials**: 

- **MATERIAL_OPAQUE** = **0**
- **MATERIAL_TRANSPARENT** = **1**
- **MATERIAL_COUNT** = **2**

enum **ColorMode**: 

- **COLOR_RAW** = **0**
- **COLOR_MESHER_PALETTE** = **1**
- **COLOR_SHADER_PALETTE** = **2**


## Property Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_color_mode"></span> **color_mode** = 0


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_greedy_meshing_enabled"></span> **greedy_meshing_enabled** = true


- [Material](https://docs.godotengine.org/en/stable/classes/class_material.html)<span id="i_opaque_material"></span> **opaque_material**


- [VoxelColorPalette](VoxelColorPalette.md)<span id="i_palette"></span> **palette**


- [Material](https://docs.godotengine.org/en/stable/classes/class_material.html)<span id="i_transparent_material"></span> **transparent_material**


## Method Descriptions

- [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_generate_mesh_from_image"></span> **generate_mesh_from_image**( [Image](https://docs.godotengine.org/en/stable/classes/class_image.html) image, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) voxel_size ) 


- [void](#)<span id="i_set_material_by_index"></span> **set_material_by_index**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) id, [Material](https://docs.godotengine.org/en/stable/classes/class_material.html) material ) 


_Generated on Oct 15, 2023_
