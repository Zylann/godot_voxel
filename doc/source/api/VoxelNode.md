# VoxelNode

Inherits: [Node3D](https://docs.godotengine.org/en/stable/classes/class_node3d.html)

Inherited by: [VoxelLodTerrain](VoxelLodTerrain.md), [VoxelTerrain](VoxelTerrain.md)

Base class for voxel volumes.

## Properties: 


Type                                                                                                                                               | Name                                         | Default 
-------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------- | --------
[ShadowCastingSetting](https://docs.godotengine.org/en/stable/classes/class_geometryinstance3d.html#enum-geometryinstance3d-shadowcastingsetting)  | [cast_shadow](#i_cast_shadow)                | 1       
[VoxelFormat](VoxelFormat.md)                                                                                                                      | [format](#i_format)                          |         
[VoxelGenerator](VoxelGenerator.md)                                                                                                                | [generator](#i_generator)                    |         
[GIMode](https://docs.godotengine.org/en/stable/classes/class_geometryinstance3d.html#enum-geometryinstance3d-gimode)                              | [gi_mode](#i_gi_mode)                        | 0       
[VoxelMesher](VoxelMesher.md)                                                                                                                      | [mesher](#i_mesher)                          |         
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                                                                               | [render_layers_mask](#i_render_layers_mask)  | 1       
[VoxelStream](VoxelStream.md)                                                                                                                      | [stream](#i_stream)                          |         
<p></p>

## Property Descriptions

### [ShadowCastingSetting](https://docs.godotengine.org/en/stable/classes/class_geometryinstance3d.html#enum-geometryinstance3d-shadowcastingsetting)<span id="i_cast_shadow"></span> **cast_shadow** = 1

Sets the shadow casting mode that meshes of the terrain will use.

### [VoxelFormat](VoxelFormat.md)<span id="i_format"></span> **format**

Overrides the default format of voxels.

WARNING: changing this will reload the terrain. If it has a stream attached with data saved with a different format, it might load improperly.

### [VoxelGenerator](VoxelGenerator.md)<span id="i_generator"></span> **generator**

Procedural generator used to load voxel blocks when not present in the stream.

### [GIMode](https://docs.godotengine.org/en/stable/classes/class_geometryinstance3d.html#enum-geometryinstance3d-gimode)<span id="i_gi_mode"></span> **gi_mode** = 0

Sets the Global Illumination mode meshes of the terrain will use.

### [VoxelMesher](VoxelMesher.md)<span id="i_mesher"></span> **mesher**

Defines how voxels are transformed into visible meshes.

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_render_layers_mask"></span> **render_layers_mask** = 1

Sets the render layers meshes of the terrain will use.

### [VoxelStream](VoxelStream.md)<span id="i_stream"></span> **stream**

Primary source of persistent voxel data. If left unassigned, the whole volume will use the generator.

_Generated on Aug 09, 2025_
