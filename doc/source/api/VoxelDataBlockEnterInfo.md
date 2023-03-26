# VoxelDataBlockEnterInfo

Inherits: [Object](https://docs.godotengine.org/en/stable/classes/class_object.html)


Information sent by a terrain when one of its data blocks enters the area of a [VoxelViewer](VoxelViewer.md).

## Description: 

Information sent by a terrain when one of its data blocks enters the area of a [VoxelViewer](VoxelViewer.md). See method VoxelTerrain._on_data_block_entered.

Instances of this class must not be stored, as they will become invalid after the call they come from.

## Methods: 


Return                                                                          | Signature                                               
------------------------------------------------------------------------------- | --------------------------------------------------------
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)          | [are_voxels_edited](#i_are_voxels_edited) ( ) const     
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)            | [get_lod_index](#i_get_lod_index) ( ) const             
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)            | [get_network_peer_id](#i_get_network_peer_id) ( ) const 
[Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)  | [get_position](#i_get_position) ( ) const               
[VoxelBuffer](VoxelBuffer.md)                                                   | [get_voxels](#i_get_voxels) ( ) const                   
<p></p>

## Method Descriptions

- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_are_voxels_edited"></span> **are_voxels_edited**( ) 

Tells if voxels in the block have ever been edited. If not, it means the same data can be obtained by running the generator.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_lod_index"></span> **get_lod_index**( ) 

Gets which LOD index the data block is in.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_network_peer_id"></span> **get_network_peer_id**( ) 

Gets the network peer ID of the [VoxelViewer](VoxelViewer.md) who caused the block to be referenced.

- [Vector3i](https://docs.godotengine.org/en/stable/classes/class_vector3i.html)<span id="i_get_position"></span> **get_position**( ) 

Gets the position of the data block, in data block coordinates (voxel coordinates are obtained by multiplying these coordinates with data block size).

- [VoxelBuffer](VoxelBuffer.md)<span id="i_get_voxels"></span> **get_voxels**( ) 

Gets access to the voxels in the block.

_Generated on Mar 26, 2023_
