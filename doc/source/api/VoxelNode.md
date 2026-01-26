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

## Methods: 


Return                                                                      | Signature                                                                                                
--------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------
[Node3D](https://docs.godotengine.org/en/stable/classes/class_node3d.html)  | [convert_to_nodes](#i_convert_to_nodes) ( [NodeConversionFlags](VoxelNode.md#enumerations) flags ) const 
<p></p>

## Enumerations: 

enum **NodeConversionFlags**: 

- <span id="i_NODE_CONVERSION_INCLUDE_INSTANCER"></span>**NODE_CONVERSION_INCLUDE_INSTANCER** = **1** --- If the terrain has a [VoxelInstancer](VoxelInstancer.md), instances spawned by it will be included in the result as a [MultiMeshInstance](https://docs.godotengine.org/en/stable/classes/class_multimeshinstance.html) per chunk.
- <span id="i_NODE_CONVERSION_INCLUDE_INVISIBLE_BLOCKS"></span>**NODE_CONVERSION_INCLUDE_INVISIBLE_BLOCKS** = **2** --- Some chunks are internally hidden by the terrain system, notably as part of the LOD system. If this flag is provided, these chunks will be included in the output.
- <span id="i_NODE_CONVERSION_INCLUDE_MATERIAL_OVERRIDES"></span>**NODE_CONVERSION_INCLUDE_MATERIAL_OVERRIDES** = **4** --- Specifies that material overrides present on the terrain should be applied to chunks of the result.


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

## Method Descriptions

### [Node3D](https://docs.godotengine.org/en/stable/classes/class_node3d.html)<span id="i_convert_to_nodes"></span> **convert_to_nodes**( [NodeConversionFlags](VoxelNode.md#enumerations) flags ) 

Generates a tree representing the terrain at the time of calling, only using Godot vanilla nodes.

This method can be used as a way to produce a snapshot of the terrain as a vanilla scene that can be opened with standard Godot versions without plugins. It could also be used for debug purposes, or as a way to workaround Godot features that expect mesh instance nodes (such as some GI baking options, navigation generation or GLTF export).

The returned root is not added to the scene tree. It is up to you to decide what to do with it. If you want it to be present in the scene, you should add it as child of an existing node. Otherwise, don't forget to free it.

Output structure: every chunk of the terrain will create its own [MeshInstance3D](https://docs.godotengine.org/en/stable/classes/class_meshinstance3d.html), with a unique [ArrayMesh](https://docs.godotengine.org/en/stable/classes/class_arraymesh.html) resource attached to it. Meshes and/or mesh instances may also have materials attached to them. Depending on how the terrain is configured, these materials can be unique per chunk. If the terrain has LODs, chunks will be stored under a corresponding LOD node. If the terrain has an instancer, multimesh chunks can be included. See [NodeConversionFlags](VoxelNode.md#enumerations) for options.

Resources: unless specified, resources attached to nodes of the tree will be direct references to those actually used by the terrain system. Careful, depending on configuration, it is possible that they get altered when the terrain runs its next process cycle (for example, materials with per-chunk parameters are pooled, so when chunks get unloaded, those referenced in the nodes might drastically change as new chunks re-use them).

Performance: this can be an expensive operation. It might slow down your game.

Packing to scene file: it is possible to save the output as a [PackedScene](https://docs.godotengine.org/en/stable/classes/class_packedscene.html), by assigning [Node.owner](https://docs.godotengine.org/en/stable/classes/class_node.html#class-node-property-owner) (see Godot's documentation). It contains a lot of unique meshes so the saved scenes can be very large. Prefer saving as binary `.scn` if it's a problem. If the terrain uses a [ShaderMaterial](https://docs.godotengine.org/en/stable/classes/class_shadermaterial.html) with detail rendering, chunks will also have unique textures, further increasing size.

GLTF: if your terrain uses [ShaderMaterial](https://docs.godotengine.org/en/stable/classes/class_shadermaterial.html) and you want to convert the terrain to a [GLTFDocument](https://docs.godotengine.org/en/stable/classes/class_gltfdocument.html), these materials won't carry over because GLTF does not support it. That also means if you use [VoxelMesherTransvoxel](VoxelMesherTransvoxel.md) with LOD, meshes in your GLTF document will have unwanted extra geometry and cracks at the seams (which would normally be handled by shader).

LOD: if your terrain has LOD, you will only get a snapshot of which LODs are currently active. Detailed meshes will be centered around viewers, and have less and less details further away.

_Generated on Jan 26, 2026_
