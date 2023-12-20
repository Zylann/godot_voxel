Voxel block format v3
====================

!!! warning
    This document is about an old version of the format. You may check the most recent version.

Version: 3

This page describes the binary format used by default in this module to serialize voxel blocks to files, network or databases.

### Changes from version 2

- The second channel (at index 1) was used for SDF data but the format didn't dictate anything particular about it. It is now expected to be for SDF. It used to have an arbitrary format for fixed-point encoding. It is now using inorm16.
- Compression format `1` is deprecated.
- Moved compression wrapper to its own specification.


Specification
----------------

### Endianess

By default, little-endian.

### Compressed container

A block is usually serialized within a compressed data container.
This is the format provided by the `VoxelBlockSerializer` utility class. If you don't use compression, the layout will correspond to `BlockData` described in the next listing, and won't have this wrapper.
See [Compressed container format](#compressed-container) for specification.

### Block format

It starts with version number `3` in one byte, then some metadata and the actual voxels.

!!! note
    The size and formats are present to make the format standalone. When used within a chunked container like region files, it is recommended to check if they match the format expected for the volume as a whole.

```
BlockData
- version: uint8_t
- size_x: uint16_t
- size_y: uint16_t
- size_z: uint16_t
- channels[8]
- metadata*
- epilogue
```

### Channels

Block data starts with exactly 8 channels one after the other, each with the following structure:

```
Channel
- format: uint8_t (low nibble = compression, high nibble = depth)
- data
```

`format` contains both compression and bit depth, respectively known as `VoxelBuffer::Compression` and `VoxelBuffer::Depth` enums. The low nibble contains compression, and the high nibble contains depth. Depending on those values, `data` will be different.

Depth can be 0 (8-bit), 1 (16-bit), 2 (32-bit) or 3 (64-bit).

If compression is `COMPRESSION_NONE` (0), `data` will be an array of N*S bytes, where N is the number of voxels inside a block, multiplied by the number of bytes corresponding to the bit depth. For example, a block of size 16x16x16 and a channel of 32-bit depth will have `16*16*16*4` bytes to load from the file into this channel.
The 3D indexing of that data is in order `ZXY`.

If compression is `COMPRESSION_UNIFORM` (1), the data will be a single voxel value, which means all voxels in the block have that same value. Unused channels will always use this mode. The value spans the same number of bytes defined by the depth.

Other compression values are invalid.

#### SDF channel

The second channel (at index 1) is used for SDF data. If depth is 8 or 16 bits, it may contain fixed-point values encoded as `inorm8` or `inorm16`. This is numbers in the range [-1..1].

To obtain a `float` from an `int8`, use `max(i / 127, -1.f)`.
To obtain a `float` from an `int16`, use `max(i / 32767, -1.f)`.

For 32-bit depth, regular `float` are used.
For 64-bit depth, regular `double` are used.

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

!!! note
    On little-endian architectures (like desktop), binary editors will not show the epilogue as `0x900df00d`, but as `0x0df00d90` instead.


Current Issues
----------------

### Endianess

The format is intented to use little-endian, however the implementation of the engine does not fully guarantee this.

Godot's `encode_variant` doesn't seem to care about endianess across architectures, so it's possible it becomes a problem in the future and gets changed to a custom format.
The implementation of block channels with depth greater than 8-bit currently doesn't consider this either. This might be refined in a later iteration.

This will become important to address if voxel games require communication between mobile and desktop.
