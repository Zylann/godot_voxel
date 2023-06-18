# VoxelBlockSerializer

Inherits: [RefCounted](https://docs.godotengine.org/en/stable/classes/class_refcounted.html)




## Description: 

Low-level utility to save and load the data within a [VoxelBuffer](VoxelBuffer.md). This can be useful to send data over the network, or to store it in a file.



To store into a file allocating a PackedByteArray:

```gdscript
# `voxels` is an existing `VoxelBuffer`
var data := VoxelBlockSerializer.serialize_to_byte_array(voxels, true)
file.store_32(len(data))
file.store_buffer(data)

```

To read it back:

```gdscript
var size := file.get_32()
var data := file.get_buffer(size)
VoxelBlockSerializer.deserialize_from_byte_array(data, voxels, true)

```



To store into a file by re-using a StreamPeerBuffer:

```gdscript
# Note, buffer can be re-used if you do this often
var stream_peer_buffer := StreamPeerBuffer.new()
var written_size = VoxelBlockSerializer.serialize_to_stream_peer(stream_peer_buffer, voxels, true)
file.store_32(written_size)
file.store_buffer(stream_peer_buffer.data_array)

```

To read it back:

```gdscript
var size := file.get_32()
var stream_peer_buffer := StreamPeerBuffer.new()
# Unfortunately Godot will always allocate memory with this API, can't avoid that
stream_peer_buffer.data_array = file.get_buffer(size)
VoxelBlockSerializer.deserialize_from_stream_peer(stream_peer_buffer, voxels, size, true)

```

## Methods: 


Return                                                                                        | Signature                                                                                                                                                                                                                                                                                                                                                                    
--------------------------------------------------------------------------------------------- | -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                                                     | [deserialize_from_byte_array](#i_deserialize_from_byte_array) ( [PackedByteArray](https://docs.godotengine.org/en/stable/classes/class_packedbytearray.html) bytes, [VoxelBuffer](VoxelBuffer.md) voxel_buffer, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) decompress ) static                                                                   
[void](#)                                                                                     | [deserialize_from_stream_peer](#i_deserialize_from_stream_peer) ( [StreamPeer](https://docs.godotengine.org/en/stable/classes/class_streampeer.html) peer, [VoxelBuffer](VoxelBuffer.md) voxel_buffer, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) size, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) decompress ) static 
[PackedByteArray](https://docs.godotengine.org/en/stable/classes/class_packedbytearray.html)  | [serialize_to_byte_array](#i_serialize_to_byte_array) ( [VoxelBuffer](VoxelBuffer.md) voxel_buffer, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) compress ) static                                                                                                                                                                                 
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                          | [serialize_to_stream_peer](#i_serialize_to_stream_peer) ( [StreamPeer](https://docs.godotengine.org/en/stable/classes/class_streampeer.html) peer, [VoxelBuffer](VoxelBuffer.md) voxel_buffer, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) compress ) static                                                                                      
<p></p>

## Method Descriptions

- [void](#)<span id="i_deserialize_from_byte_array"></span> **deserialize_from_byte_array**( [PackedByteArray](https://docs.godotengine.org/en/stable/classes/class_packedbytearray.html) bytes, [VoxelBuffer](VoxelBuffer.md) voxel_buffer, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) decompress ) 

Reads the data of a [VoxelBuffer](VoxelBuffer.md) from a [PackedByteArray](https://docs.godotengine.org/en/stable/classes/class_packedbytearray.html).

- [void](#)<span id="i_deserialize_from_stream_peer"></span> **deserialize_from_stream_peer**( [StreamPeer](https://docs.godotengine.org/en/stable/classes/class_streampeer.html) peer, [VoxelBuffer](VoxelBuffer.md) voxel_buffer, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) size, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) decompress ) 

Reads the data of a [VoxelBuffer](VoxelBuffer.md) from a [StreamPeer](https://docs.godotengine.org/en/stable/classes/class_streampeer.html). You must provide the number of bytes to read.

- [PackedByteArray](https://docs.godotengine.org/en/stable/classes/class_packedbytearray.html)<span id="i_serialize_to_byte_array"></span> **serialize_to_byte_array**( [VoxelBuffer](VoxelBuffer.md) voxel_buffer, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) compress ) 

Stores the data of a [VoxelBuffer](VoxelBuffer.md) into a [PackedByteArray](https://docs.godotengine.org/en/stable/classes/class_packedbytearray.html).

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_serialize_to_stream_peer"></span> **serialize_to_stream_peer**( [StreamPeer](https://docs.godotengine.org/en/stable/classes/class_streampeer.html) peer, [VoxelBuffer](VoxelBuffer.md) voxel_buffer, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) compress ) 

Stores the data of a [VoxelBuffer](VoxelBuffer.md) into a [StreamPeer](https://docs.godotengine.org/en/stable/classes/class_streampeer.html). Returns the number of written bytes.

_Generated on Jun 18, 2023_
