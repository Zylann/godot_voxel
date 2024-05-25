SQLite format
================

This page describes the database schema used by `VoxelStreamSQLite`.


Changes from version 0
-------------------------

Added a `coordinate_format` column to the `meta` table, and several ways to represent coordinates in the `blocks` table.


Schema
--------

### `meta`

```
meta {
    - version: INTEGER
    - block_size_po2: INTEGER
    - coordinate_format: INTEGER
}
```

Contains general info about the volume. There is only one row inside it.

- `version` is the version of the schema. Currently `1`.
- `block_size_po2` is the size of blocks as a power of two. They are expected to be always the same. By default it is `4` (for blocks of 16x16x16).
- `coordinate_format` specifies how block coordinates are stored.

Usually, this row should never be modified once the database is setup. If for some reason changes are necessary, they must be done such that the database remains consistent (and may require to re-process all the blocks). Creating a new database and converting over may be preferable than modifying in-place.


### `blocks`

```
blocks {
    if meta.coordinate_format is:
        0 or 1
            - loc: INT64 PRIMARY KEY
        2:
            - loc: TEXT PRIMARY KEY

    - vb: BLOB
    - instances: BLOB
}
```

Contains every block of the volume. There can be thousands of them.

- `loc` is a key identifying the block, usually made from its coordinates. Its encoding depends on `meta.coordinate_format`.
- `vb` contains compressed voxel data using the [Block format](block_format_v4.md).
- `instances` contains compressed instance data using the [Instance format](instances_format_v1.md).

#### Coordinate format

In all cases, coordinates are equal to the origin of the block in voxels, divided by the size of the block + lod index using euclidean division (`coord >> (block_size_po2 + lod_index)`).
Depending on `meta.coordinate_format`, that column is interpreted differently:

- `0`: 64-bit little-endian integer packing the coordinates and LOD index of the block. XYZ are 16-bit signed integers, and LOD is a 8-bit unsigned integer: `0LXXYYZZ`. This was the default format in v0.
- `1`: 64-bit little-endian integer packing the coordinates and LOD index of the block. XYZ are 19-bit signed integers, and LOD is a 7-bit unsigned integer: `lllllllx xxxxxxxx xxxxxxxx xxyyyyyy yyyyyyyy yyyyyzzz zzzzzzzz zzzzzzzz` (where the most significant bits are on the left).
- `2`: Comma-separated coordinates in base 10, stored in plain text, without spaces.
- `3`: 80-bit blob packing the coordinates and LOD index. XYZ are 25-bit signed integers, and LOD is a 5-bit unsigned integer. 

Format `3` can be represented this way:
```
Byte |   9        8        7        6        5        4        3        2        1        0
-----|--------|--------|--------|--------|--------|--------|--------|--------|--------|--------
Bits |lllllzzz zzzzzzzz zzzzzzzz zzzzzzyy yyyyyyyy yyyyyyyy yyyyyyyx xxxxxxxx xxxxxxxx xxxxxxxx
```
Where each cluster of bits (for each coordinate) may be read with most significant bit to the left, as when printed out. Note the reverse byte order.



### `channels`

```
channels {
    - idx: INTEGER PRIMARY KEY
    - depth: INTEGER
}
```

Contains general info about which channel formats should be expected in the volume. There is one row per used channel.

!!! warning
    Currently this table is actually not used, because the engine still needs work to manage formats in general. For now the database accepts blocks of any formats since they are standalone since version 3, but ideally they must be consistent.

