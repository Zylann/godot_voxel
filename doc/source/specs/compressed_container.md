Compressed data format
========================

Some custom formats used in this engine can be wrapped in a compressed container.

Specification
----------------

### Endianess

By default, little-endian.

### Compressed container

```
CompressedData
- uint8_t format
- data
```

Compressed data starts with one byte. Depending on its value, what follows is different.

- `0`: no compression. Following bytes can be read directly. This is rarely used and could be for debugging.
- `1`: LZ4_BE compression, *deprecated*. The next big-endian 32-bit unsigned integer is the size of the decompressed data, and following bytes are compressed data using LZ4 default parameters.
- `2`: LZ4 compression, The next little-endian 32-bit unsigned integer is the size of the decompressed data, and following bytes are compressed data using LZ4 default parameters. This is the default mode.

!!! note
    Depending on the type of data, knowing its decompressed size may be important when parsing the it later.

