# VoxelBlockSerializer

Inherits: [Reference](https://docs.godotengine.org/en/stable/classes/class_reference.html)




## Description: 

Low-level utility to save and load the data within a [VoxelBuffer](VoxelBuffer.md). This can be useful to send data over the network, or to store it in a file.

Note: this utility uses [StreamPeer](https://docs.godotengine.org/en/stable/classes/class_streampeer.html) as temporary support for the data, because Godot does not have a common interface between files and network streams. So typically, you should use a temporary [StreamPeerBuffer](https://docs.godotengine.org/en/stable/classes/class_streampeerbuffer.html) to put voxel data into, and then you can use its member StreamPeerBuffer.data_array to save the data in the actual place you want.

To store into a file:

```gdscript
# Note, buffer can be re-used if you do this often
var stream_peer_buffer = StreamPeerBuffer.new()
var written_size = serializer.serialize(stream_peer_buffer, voxels, true)
file.store_32(written_size)
file.store_buffer(stream_peer_buffer.data_array)

```

To read it back:

```gdscript
var size = file.get_32()
var stream_peer_buffer = StreamPeerBuffer.new()
# Unfortunately Godot will always allocate memory with this API, can't avoid that
stream_peer_buffer.data_array = file.get_buffer(size)
serializer.deserialize(stream_peer_buffer, voxels, size, true)

```

## Methods: 


Return                                                                | Signature                                                                                                                                                                                                                                                                                                                            
--------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[void](#)                                                             | [deserialize](#i_deserialize) ( [StreamPeer](https://docs.godotengine.org/en/stable/classes/class_streampeer.html) peer, [VoxelBuffer](VoxelBuffer.md) voxel_buffer, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) size, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) decompress )  
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [serialize](#i_serialize) ( [StreamPeer](https://docs.godotengine.org/en/stable/classes/class_streampeer.html) peer, [VoxelBuffer](VoxelBuffer.md) voxel_buffer, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) compress )                                                                                   
<p></p>

## Method Descriptions

- [void](#)<span id="i_deserialize"></span> **deserialize**( [StreamPeer](https://docs.godotengine.org/en/stable/classes/class_streampeer.html) peer, [VoxelBuffer](VoxelBuffer.md) voxel_buffer, [int](https://docs.godotengine.org/en/stable/classes/class_int.html) size, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) decompress ) 

Reads the data of a [VoxelBuffer](VoxelBuffer.md) from a [StreamPeer](https://docs.godotengine.org/en/stable/classes/class_streampeer.html). You must provide the number of bytes to read, and the destination buffer must have the expected size.

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_serialize"></span> **serialize**( [StreamPeer](https://docs.godotengine.org/en/stable/classes/class_streampeer.html) peer, [VoxelBuffer](VoxelBuffer.md) voxel_buffer, [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html) compress ) 

Stores the data of a [VoxelBuffer](VoxelBuffer.md) into a [StreamPeer](https://docs.godotengine.org/en/stable/classes/class_streampeer.html).

_Generated on Nov 06, 2021_
