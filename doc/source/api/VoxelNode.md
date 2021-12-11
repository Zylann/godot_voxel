# VoxelNode

Inherits: [Spatial](https://docs.godotengine.org/en/stable/classes/class_spatial.html)


Base class for voxel volumes.

## Properties: 


Type              | Name                       | Default 
----------------- | -------------------------- | --------
`VoxelGenerator`  | [generator](#i_generator)  |         
`VoxelMesher`     | [mesher](#i_mesher)        |         
`VoxelStream`     | [stream](#i_stream)        |         
<p></p>

## Property Descriptions

- [VoxelGenerator](VoxelGenerator.md)<span id="i_generator"></span> **generator**

Procedural generator used to load voxel blocks when not present in the stream.

- [VoxelMesher](VoxelMesher.md)<span id="i_mesher"></span> **mesher**

Defines how voxels are transformed into visible meshes.

- [VoxelStream](VoxelStream.md)<span id="i_stream"></span> **stream**

Primary source of persistent voxel data. If left unassigned, the whole volume will use the generator.

_Generated on Nov 06, 2021_
