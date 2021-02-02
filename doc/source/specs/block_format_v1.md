Block format
====================

!!! warn
    This document is about an old version of the format. You may check the most recent version.

This page describes the binary format used by default in this module to serialize voxel blocks to files, network or databases.

This format has no version (version 1 is assumed).


Specification
----------------

### Top-levels

A block is serialized as compressed data.
This is the format provided by the `VoxelBlockSerializer` utility class. If you don't use compression, the layout will correspond to `BlockData` described in the next listing.

```
CompressedBlockData
- decompressed_data_size: uint32_t
- compressed_data
```

`compressed_data` must be decompressed using the LZ4 algorithm (without header), into a buffer big enough to contain `decompressed_data_size` bytes. Knowing that size is also important later on.

The obtained data then contains the actual block. It is saved as it comes, and doesn't contain metadata about its version, 3D size or the depth of each channel. If you use this for custom block serialization, you must take care of using a format known in advance when encoding and decoding, or prepending it to the data. Region files declare block format globally.

```
BlockData
- channels[8]
- metadata*
- epilogue
```

### Channels

Block data starts with 8 channels one after the other, each with the following structure:

```
Channel
- compression: uint8_t
- data
```

`compression` is the same as the `VoxelBuffer::Compression` enum.
Depending on the value of `compression`, `data` will be different.

If compression is `COMPRESSION_NONE` (0), the data will be an array of N bytes, where N is the number of voxels inside a block, multiplied by the number of bytes in the depth setting of the current channel (defined in the meta file seen earlier). For example, a block of size 16 and a channel of 32-bit depth will have `16*16*16*4` bytes to load from the file into this channel.
The 3D indexing of that data is also in order `ZXY`.

If compression is `COMPRESSION_UNIFORM` (1), the data will be a single voxel value, which means all voxels in the block have that same value. Unused channels will always use this mode. The value can span a variable number of bytes depending on the depth of the current channel:
- 1 byte if 8-bit
- 2 bytes if 16-bits
- 4 bytes if 32-bits
- 8 bytes if 64-bits

Other compression values are invalid.

### Metadata

After all channels information, block data can contain metadata information. Blocks that don't contain any will only have a fixed amount of bytes left (from the epilogue) before reaching the size of the total data to read. If there is more, the block contains metadata.

```
Metadata
- metadata_size: uint32_t
- block_metadata
- voxel_metadata[*]
```

It starts with one 32-bit unsigned integer representing the total size of all metadata there is to read. That data comes in two groups: one for the whole block, and one per voxel.

Block metadata is one Godot `Variant`, encoded using the `encode_variant` method of the engine.

Voxel metadata immediately follows. It is a sequence of the following data structures, which must be read until a total of `metadata_size` bytes have been read from the beginning:

```
VoxelMetadata
- x: uint16_t
- y: uint16_t
- z: uint16_t
- data
```

`x`, `y` and `z` indicate which voxel the data corresponds. `data` is also a `Variant` encoded the same way as described earlier. This results in an associative collection between voxel positions relative to the block and their corresponding metadata.

### Epilogue

At the very end, block data finishes with a sequence of 4 bytes, which once read into a `uint32_t` integer must match the value `0x900df00d`. If that condition isn't fulfilled, the block must be assumed corrupted.


Current Issues
----------------

Although this format is currently implemented and usable, it has known issues.

### Endianess

Godot's `encode_variant` doesn't seem to care about endianess across architectures, so it's possible it becomes a problem in the future and gets changed to a custom format.
The rest of this spec is not affected by this and assumes we use little-endian, however the implementation of block channels currently doesn't consider this either. This may be refined in a later iteration.

### Absence of metadata

A block can't be deserialized without external information, the format must be known in advance. In the future it may be added.

### Versioning

This format doesn't have a header with version tag, which may be problematic if it changes.

User versionning might also be considered as a second layer: if the game needs to replace some metadata with new ones, or swap voxel IDs around due to a change in the game, it is desirable to expose a hook to migrate old versions. 
