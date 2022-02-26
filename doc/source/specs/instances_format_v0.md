Instance block format
=======================

!!! warn
    This document is about an old version of the format. You may check the most recent version.

This page describes the binary format used by the module to save instances to files or databases.

Specification
---------------

### Compressed container

A block is usually serialized as compressed data.
See [Compressed container format](#compressed-container) for specification.


### Binary data

This data uses big-endian.

In pseudo-code:

```cpp
// Root structure
struct InstanceBlockData {
	// Version tag in case more stuff is added in the future
	uint8_t version = 0;
    // There can be up to 256 different layers in one block
	uint8_t layer_count;
	// To compress positions we need to know their range.
	// It's local to the block so we know it starts from zero.
	float position_range;
	LayerData layers[layer_count];
	// Magic number to signal the end of the data block
	uint32_t control_end = 0x900df00d;
};

struct LayerData {
	uint16_t id; // Identifies the type of instances (rocks, grass, pebbles, bushes etc)
	uint16_t count;
	// To be able to compress scale we must know its range
	float scale_min;
	float scale_max;
	// This tells which format instances of this layer use. For now I always use the same format,
	// But maybe some types of instances will need more, or less data?
	uint8_t format = 0;
	InstanceData data[count];
};

struct InstanceData {
	// Position is lossy-compressed based on the size of the block
	uint16_t x;
	uint16_t y;
	uint16_t z;
	// Scale is uniform and is lossy-compressed to 256 values
	uint8_t scale;
	// Rotation is a compressed quaternion
	uint8_t x;
	uint8_t y;
	uint8_t z;
	uint8_t w;
};
```
