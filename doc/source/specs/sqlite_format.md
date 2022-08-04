SQLite format
================

This page describes the database schema used by `VoxelStreamSQLite`.


Schema
--------

### `meta`

```
meta {
    - version: INTEGER
    - block_size_po2: INTEGER
}
```

Contains general info about the volume. There is only one row inside it.

- `version` is the version of the schema. Currently `0`.
- `block_size_po2` is the size of blocks as a power of two. They are expected to be always the same. By default it is `4` (for blocks of 16x16x16).


### `blocks`

```
blocks {
    - loc: INT64 PRIMARY KEY
    - vb: BLOB
    - instances: BLOB
}
```

Contains every block of the volume. There can be thousands of them.

- `loc` is a 64-bit integer packing the coordinates and LOD index of the block using little-endian. Coordinates are equal to the origin of the block in voxels, divided by the size of the block + lod index using euclidean division (`coord >> (block_size_po2 + lod_index)`). XYZ are 16-bit signed integers, and LOD is a 8-bit unsigned integer: `0LXXYYZZ`
- `vb` contains compressed voxel data using the [Block format](block_format_v4.md).
- `instances` contains compressed instance data using the [Instance format](instances_format.md).


### `channels`

```
channels {
    - idx: INTEGER PRIMARY KEY
    - depth: INTEGER
}
```

Contains general info about which channel formats should be expected in the volume. There is one row per used channel.

!!! warn
    Currently this table is actually not used, because the engine still needs work to manage formats in general. For now the database accepts blocks of any formats since they are standalone since version 3, but ideally they must be consistent.

