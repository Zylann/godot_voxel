Region format
==================

Version: 3

Region files allows to save large fixed-size 3D voxel volumes in a format suitable for frequent streaming and partial edition.
This format is inspired by [Seed of Andromeda](https://www.seedofandromeda.com/blogs/1-creating-a-region-file-system-for-a-voxel-game) and Minecraft.
It is used by `VoxelStreamRegionFiles`, which is implemented in [this C++ file](https://github.com/Zylann/godot_voxel/blob/master/streams/voxel_stream_region_files.cpp)

Two use cases exist:
- Standalone region: fixed-size voxel volume
- Region forest: using multiple region files for infinite voxel worlds without boundaries. This used to be the only case region files were used for.

!!! note
	The "Region" name in this document does not designate a standard, but an approach. The format described here is specific to the Godot module, and could be referred to as `Godot Voxel VXR` if a full name is needed.


Migration
-----------

Older saves made using this format can be migrated if they use version 2.

### Changes in version 3

Information about block size, voxel format and palette was added, so that a standalone region file contains all the necessary information to load and save voxel data. Before, this information had to be known in advance by the user.
Migration will insert extra bytes and offset the rest of the file, and will write a new header over.


Coordinate spaces
-------------------

This document uses 3 different coordinate spaces. Each one can be converted to another by using a multiplier.

- Voxel coordinates: actual position of voxels in space
- Block coordinates: position of a block of voxels with a defined size B. For example, common block size is 16x16x16 voxels. Block coordinates can be converted into voxel coordinates by multiplying it by B, giving the origin voxel within that block.
- Region coordinates: position of a region of blocks with a defined size R. A region coordinate can be converted into block coordinates by multiplying it by R, giving the origin block within that region.

Powers of two may be used as multipliers.


Region forest
----------------

### Filesystem structure

A region forest is organized in multiple region files, and is contained within a root directory containing them. Region files don't need to be inside a forest to be usable.
Under that directory, is located two things:

- A `meta.vxrm` file
- A `regions` directory

Under the region directory, there must be a sub-directory, for each layer of level of detail (LOD). Those folders must be named `lodX`, where `X` is the LOD index, starting from `0`.

LOD folders then contain region files for that LOD.
Each region file is named using the following convention: `r.X.Y.Z.vxr`, where X, Y and Z are coordinates of the region, in the region coordinate space.

- `world/`
	- `meta.vxrm`
	- `regions/`
		- `lod0/`
			- `r.0.0.0.vxr`
			- `r.1.6.0.vxr`
			- `r.32.-2.-6.vxr`
			- ...
		- `lod1/`
			- ...
		- `lod2/`
			- ...
		- ...


### Meta file

The meta file under the root directory contains global information about all voxel data. It is currently using JSON, but may not be edited by hand.

It must contain the following fields:

- `version`: integer telling the version of that format. It must be `3`. Older versions may be migrated.
- `block_size_po2`: size of blocks in voxels, as an integer power of two (4 for 16, 5 for 32 etc). Blocks are always cubic.
- `lod_count`: how many LOD levels there are. There will be as many LOD folders. It must be greater than 0.
- `region_size_po2`: size of regions in blocks, as an integer power of two (4 for 16, 5 for 32 etc). Regions are always cubic.
- `sector_size`: size of a sector within a region file, as a strictly positive integer. See region format for more information.
- `channel_depths`: array of 8 integers, representing the bit depth of each voxel channel:
	- `0`: 8 bits
	- `1`: 16 bits
	- `2`: 32 bits
	- `3`: 64 bits
	- See block format for more information.


Region file
-------------

Region files are binary, little-endian. They are composed of a prologue, header, and sector data.

```
Prologue:
- "VXR_"
- version: uint8_t
Header:
- block_size_po2: uint8_t // cubic size of the block as a power of two. Must not be zero.
- region_size_x: uint8_t // How many blocks the region spans across X
- region_size_y: uint8_t // How many blocks the region spans across Y
- region_size_z: uint8_t // How many blocks the region spans across Z
- channel_depths: uint8_t[8] // Channel depths, same as described in region forest meta files
- sector_size: uint16_t
- palette_hint: uint8_t
- palette: uint32_t[256]
- blocks: uint32_t[region_size ^ 3]
SectorData:
- ...
```

### Prologue

It starts with four 8-bit characters: `VXR_`, followed by one byte representing the version of the format in binary form. The version must be `3`.

### Header

The header starts with some metadata describing the size of the volume and the format of voxels. It no longer has a fixed size.

A color palette can be optionally provided. If `palette_hint` is set to `0xff` (`255`), it must be followed by 256 8-bit RGBA values. If `palette_hint` is `0x00` (`0`), then no palette data will follow. Other values are invalid at the moment.

`blocks` is a sequence of 32-bit integers, located at the end of the header. Each integer represents information about where a block is in the file, and how big its serialized data is. The count of that sequence is the number of blocks a region can contain, and remains constant for a given region size. The index of elements in that sequence is calculated from 3D block positions, in ZXY order. The index for a block can be obtained with the formula `y + block_size * (x + block_size * z)`.
Each integer contains two informations:
- The first byte is the number of sectors the block is spanning. Obtained as `n & 0xff`.
- The 3 other bytes are the index to the first sector. Obtained as `n >> 8`.

As a result, if a block is unoccupied, its value is `0`.

### Sectors

The rest of the file is occupied by sectors.
Sectors are fixed-size chunks of data. Their size is determined from the header described earlier, and also in a meta file if part of a region forest.
Blocks are stored in those sectors. A block can span one or more sectors.
The file is partitioned in this way to allow frequently writing blocks of variable size without having to often shift consecutive contents.

When we need to load a block, the address where block information starts will be the following:
```
header_size + first_sector_index * sector_size
```

Once we have the address of the block, the first 4 bytes at this address will contain the size of the written data.
Note: those 4 bytes are included in the total block size when the number of occupied sectors is determined.

```
RegionBlockData
- buffer_size: uint32_t
- buffer
```

The obtained buffer can be read using the block format.


Block format
--------------

See [Block format](block_format_v2.md)


Current Issues
----------------

Although this format is currently implemented and usable, it has known issues.

### Endianess

Godot's `encode_variant` doesn't seem to care about endianess across architectures, so it's possible it becomes a problem in the future and gets changed to a custom format.
The rest of this spec is not affected by this and assumes we use little-endian, however the implementation of block channels currently doesn't consider this either. This may be refined in a later iteration.

### Versioning

The region format should be thought of a container for instances of the block format. The former has a version number, but the latter doesn't, which is hard to manage. We may introduce separate versionning, which will cause older saves to become incompatible.

User versionning may also be added as a third layer: if the game needs to replace some metadata with new ones, or swap voxel IDs around due to a change in the game, it is desirable to expose a hook to migrate old versions.
