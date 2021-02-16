Region format
==================

!!! warn
    This document is about an old version of the format. You may check the most recent version.

Version: 2

Regions allows to save large 3D voxel volumes in a format suitable for frequent streaming in all directions.  
This format is inspired by https://www.seedofandromeda.com/blogs/1-creating-a-region-file-system-for-a-voxel-game  
It is implemented by `VoxelStreamRegionFiles`, which can be found in https://github.com/Zylann/godot_voxel/blob/master/streams/voxel_stream_region_files.cpp


Coordinate spaces
-------------------

This format uses 3 different coordinate spaces. Each one can be converted to another by using a multiplier.

- Voxel coordinates: actual position of voxels in space
- Block coordinates: position of a block of voxels with a defined size B. For example, common block size is 16x16x16 voxels. Block coordinates can be converted into voxel coordinates by multiplying it by B, giving the origin voxel within that block.
- Region coordinates: position of a region of blocks with a defined size R. A region coordinate can be converted into block coordinates by multiplying it by R, giving the origin block within that region.

Powers of two may be used as multipliers.


File structure
----------------

A region save is organized in multiple files, and is contained within a root directory containing them.
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


Meta file
------------

The meta file under the root directory contains global information about all voxel data. It is currently using JSON, but may not be edited by hand.

It must contain the following fields:

- `version`: integer telling the version of that format. It must be `2`. Older versions may be migrated.
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
- blocks: uint32_t[region_size ^ 3]
SectorData:
- ...
```

### Prologue

It starts with four 8-bit characters: `VXR_`, followed by one byte representing the version of the format in binary form. The version must be `2`. Version `1` has the same data layout so it can be read the same. Other versions cannot be read.

### Header

The header is a sequence of 32-bit integers. Each integer represents information about where a block is in the file, and how big it is. The count of that sequence is the number of blocks a region can contain, and is the same in all regions. The index of elements in that sequence is calculated from 3D block positions, in ZXY order. The index for a block can be obtained with the formula `y + block_size * (x + block_size * z)`.
Each integer contains two informations:
- The first byte is the number of sectors the block is spanning. Obtained as `n & 0xff`.
- The 3 other bytes are the index to the first sector. Obtained as `n >> 8`.

### Sectors

The rest of the file is occupied by sectors.
Sectors are fixed-size chunks of data. Their size is determined from the meta file described earlier.
Blocks are stored in those sectors. A block can span one or more sectors.
The file is partitionned in this way to allow frequently writing blocks of variable size without having to often shift consecutive contents.

When we need to load a block, the address where block information starts will be the following:
```
header_size + first_sector_index * sector_size
```

Once we have the address of the block, the first 4 bytes at this address will contain the size of the written data.

!!! note
	those 4 bytes are included in the total block size when the number of occupied sectors is determined.

```
RegionBlockData
- buffer_size: uint32_t
- buffer
```

The obtained buffer can be read using the block format.


Block format
--------------

See [Block format](block_format_v1.md)


Current Issues
----------------

Although this format is currently implemented and usable, it has known issues.

### Endianess

Godot's `encode_variant` doesn't seem to care about endianess across architectures, so it's possible it becomes a problem in the future and gets changed to a custom format.
The rest of this spec is not affected by this and assumes we use little-endian, however the implementation of block channels currently doesn't consider this either. This may be refined in a later iteration.

### Versioning

The region format should be thought of a container for instances of the block format. The former has a version number, but the latter doesn't, which is hard to manage. We may introduce separate versionning, which will cause older saves to become incompatible.

User versionning may also be added as a third layer: if the game needs to replace some metadata with new ones, or swap voxel IDs around due to a change in the game, it is desirable to expose a hook to migrate old versions.
