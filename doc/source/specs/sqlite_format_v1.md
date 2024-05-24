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

In all cases, coordinates are equal to the origin of the block in voxels, divided by the size of the block + lod index using euclidean division (`coord >> (block_size_po2 + lod_index)`). Byte order is little-endian.
Depending on `meta.coordinate_format`, that column is interpreted differently:

- `0`: 64-bit integer packing the coordinates and LOD index of the block. XYZ are 16-bit signed integers, and LOD is a 8-bit unsigned integer: `0LXXYYZZ`. This was the default format in v0.
- `1`: 64-bit integer packing the coordinates and LOD index of the block. XYZ are 19-bit signed integers, and LOD is a 7-bit unsigned integer: `lllllllx xxxxxxxx xxxxxxxx xxyyyyyy yyyyyyyy yyyyyzzz zzzzzzzz zzzzzzzz`.
- `2`: Comma-separated coordinates in base 10, stored in plain text, without spaces.
- `3`: 80-bit blob packing the coordinates and LOD index. XYZ are 25-bit signed integers, and LOD is a 5-bit unsigned integer. 

Format `3` can be represented this way:
```
Byte |   0        1        2        3        4        5        6        7        8        9
-----|--------|--------|--------|--------|--------|--------|--------|--------|--------|--------
Bits |xxxxxxxx xxxxxxxx xxxxxxxx yyyyyyyx yyyyyyyy yyyyyyyy zzzzzzyy zzzzzzzz zzzzzzzz lllllzzz
```

TODO Format 3 representation is ambiguous, each next byte actually contains more significant bits instead of following a human-readable form from left to right


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

