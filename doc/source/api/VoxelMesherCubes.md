# VoxelMesherCubes

Inherits: [VoxelMesher](VoxelMesher.md)

Generates a cubic voxels mesh from colors.

## Description: 

Builds meshes representing cubic voxels, based on color values stored in the [VoxelBuffer.CHANNEL_COLOR](VoxelBuffer.md#i_CHANNEL_COLOR) channel. 

Colors will usually be stored in the `COLOR` vertex attribute and may require a material with vertex colors enabled, or a custom shader.

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

- <span id="i_MATERIAL_OPAQUE"></span>**MATERIAL_OPAQUE** = **0** --- Index of the opaque material.
- <span id="i_MATERIAL_TRANSPARENT"></span>**MATERIAL_TRANSPARENT** = **1** --- Index of the transparent material.
- <span id="i_MATERIAL_COUNT"></span>**MATERIAL_COUNT** = **2** --- Maximum number of materials.

enum **ColorMode**: 

- <span id="i_COLOR_RAW"></span>**COLOR_RAW** = **0** --- Voxel values will be directly interpreted as colors. 8-bit voxels are interpreted as `rrggbbaa` (2 bits per component) where the range per component is converted from 0..3 to 0..255. 16-bit voxels are interpreted as `rrrrgggg bbbbaaaa` (4 bits per component) where the range per component is converted from 0..15 to 0..255. 32-bit voxels are interpreted as `rrrrrrrr gggggggg bbbbbbbb aaaaaaaa` (8 bits per component) where each component is in 0..255.
- <span id="i_COLOR_MESHER_PALETTE"></span>**COLOR_MESHER_PALETTE** = **1** --- Voxel values will be interpreted as indices within the color palette assigned in the [VoxelMesherCubes.palette](VoxelMesherCubes.md#i_palette) property.
- <span id="i_COLOR_SHADER_PALETTE"></span>**COLOR_SHADER_PALETTE** = **2** --- Voxel values will be directly written as such in the mesh, instead of colors. They are written in the red component of the `COLOR`, leaving red and blue to zero. Note, it will be normalized to 0..1 in shader, so if you need the integer value back you may use `int(COLOR.r * 255.0)`. The alpha component will be set to the transparency of the corresponding color in [VoxelMesherCubes.palette](VoxelMesherCubes.md#i_palette) (a palette resource is still needed to differenciate transparent parts; RGB values are not used). You are expected to use a [ShaderMaterial](https://docs.godotengine.org/en/stable/classes/class_shadermaterial.html) to read vertex data and choose the actual color with a custom shader. [StandardMaterial](https://docs.godotengine.org/en/stable/classes/class_standardmaterial.html) will not work with this mode.


## Property Descriptions

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_color_mode"></span> **color_mode** = 0

Sets how voxel color is determined when building the mesh.

### [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_greedy_meshing_enabled"></span> **greedy_meshing_enabled** = true

Enables greedy meshing: the mesher will attempt to merge contiguous faces having the same color to reduce the number of polygons.

### [Material](https://docs.godotengine.org/en/stable/classes/class_material.html)<span id="i_opaque_material"></span> **opaque_material**

Material that will be used for opaque parts of the mesh.

### [VoxelColorPalette](VoxelColorPalette.md)<span id="i_palette"></span> **palette**

Palette that will be used when using the [VoxelMesherCubes.COLOR_MESHER_PALETTE](VoxelMesherCubes.md#i_COLOR_MESHER_PALETTE) color mode.

### [Material](https://docs.godotengine.org/en/stable/classes/class_material.html)<span id="i_transparent_material"></span> **transparent_material**

Material that will be used for transparent parts of the mesh (colors where alpha is not set to max).

## Method Descriptions

### [Mesh](https://docs.godotengine.org/en/stable/classes/class_mesh.html)<span id="i_generate_mesh_from_image"></span> **generate_mesh_from_image**( [Image](https://docs.godotengine.org/en/stable/classes/class_image.html) image, [float](https://docs.godotengine.org/en/stable/classes/class_float.html) voxel_size ) 

Generates a 1-voxel thick greedy mesh from pixels of an image.

### [void](#)<span id="i_set_material_by_index"></span> **set_material_by_index**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) id, [Material](https://docs.godotengine.org/en/stable/classes/class_material.html) material ) 

Sets one of the materials that will be used when building meshes. This is equivalent to using either [VoxelMesherCubes.opaque_material](VoxelMesherCubes.md#i_opaque_material) or [VoxelMesherCubes.transparent_material](VoxelMesherCubes.md#i_transparent_material).

_Generated on Aug 27, 2024_
