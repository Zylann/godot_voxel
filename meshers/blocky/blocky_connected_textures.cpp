#include "blocky_connected_textures.h"
#include "../../util/containers/span.h"
#include "../../util/godot/classes/image.h"
#include "../../util/godot/core/rect2i.h"
#include "../../util/math/vector2i.h"
#include <cstdint>

namespace zylann::voxel::blocky {

// 47 cases
// - - -   - - -   - - -   - - -   - - -   - - -   - o -   - - -   - o o   - o -   o o -   o o o
// - x -   - x o   o x o   o x -   - x o   o x -   - x o   o x o   o x o   o x o   o x o   o x o
// - - -   - - -   - - -   - - -   - o -   - o -   - o -   - o -   - o -   - o o   o o -   - o -
//   0       1       2       3       4       5       6       7       8       9       10      11
//
// - - -   - - -   - - -   - - -   - o -   - o -   - o -   - o -   o o -   - o -   - o -   - o o
// - x -   - x o   o x o   o x -   - x o   o x -   o x o   o x -   o x o   o x o   o x o   o x o
// - o -   - o o   o o o   o o -   - - -   - - -   - - -   - o -   - o -   o o -   o o o   - o o
//   12      13      14      15      16      17      18      19      20      21      22      23
//
// - o -   - o o   o o o   o o -   - o -   - - -   - o o   - - -   o o o   o o o   - o o   o o -
// - x -   - x o   o x o   o x -   - x o   o x o   - x o   o x o   o x o   o x o   o x o   o x o
// - o -   - o o   o o o   o o -   - o o   o o -   - o -   - o o   o o -   - o o   o o -   - o o
//   24      25      26      27      28      29      30      31      32      33      34      35
//
// - o -   - o o   o o o   o o -   - o o   o o -   o o -   - o -   o o -   - o o   - o -   - - -
// - x -   - x o   o x o   o x -   o x o   o x -   o x o   o x -   o x o   o x o   o x o   - - -
// - - -   - - -   - - -   - - -   - - -   - o -   - - -   o o -   o o o   o o o   - o -   - - -
//   36      37      38      39      40      41      42      43      44      45      46

// clang-format off
static const std::array<Vector2i, BLOB9_TILE_COUNT> g_default_layout_tile_positions = {
	Vector2i(0, 0), // 0
	Vector2i(1, 0), // 1
	Vector2i(2, 0), // 2
	Vector2i(3, 0), // 3
	Vector2i(4, 0), // 4
	Vector2i(5, 0), // 5
	Vector2i(6, 0), // 6
	Vector2i(7, 0), // 7
	Vector2i(8, 0), // 8
	Vector2i(9, 0), // 9
	Vector2i(10, 0), // 10
	Vector2i(11, 0), // 11
	Vector2i(0, 1), // 12
	Vector2i(1, 1), // 13
	Vector2i(2, 1), // 14
	Vector2i(3, 1), // 15
	Vector2i(4, 1), // 16
	Vector2i(5, 1), // 17
	Vector2i(6, 1), // 18
	Vector2i(7, 1), // 19
	Vector2i(8, 1), // 20
	Vector2i(9, 1), // 21
	Vector2i(10, 1), // 22
	Vector2i(11, 1), // 23
	Vector2i(0, 2), // 24
	Vector2i(1, 2), // 25
	Vector2i(2, 2), // 26
	Vector2i(3, 2), // 27
	Vector2i(4, 2), // 28
	Vector2i(5, 2), // 29
	Vector2i(6, 2), // 30
	Vector2i(7, 2), // 31
	Vector2i(8, 2), // 32
	Vector2i(9, 2), // 33
	Vector2i(10, 2), // 34
	Vector2i(11, 2), // 35
	Vector2i(0, 3), // 36
	Vector2i(1, 3), // 37
	Vector2i(2, 3), // 38
	Vector2i(3, 3), // 39
	Vector2i(4, 3), // 40
	Vector2i(5, 3), // 41
	Vector2i(6, 3), // 42
	Vector2i(7, 3), // 43
	Vector2i(8, 3), // 44
	Vector2i(9, 3), // 45
	Vector2i(10, 3) // 46
};
// clang-format on

// 0 1 2
// 3 x 4
// 5 6 7
//
// clang-format off
static const uint8_t g_connection_mask_to_case_index[256] = {
	0, 0, 36, 36, 0, 0, 36, 36, 3, 3, 17, 39, 3, 3, 17, 39, // 0..15
	1, 1, 16, 16, 1, 1, 37, 37, 2, 2, 18, 42, 2, 2, 40, 38, // 16..31
	0, 0, 36, 36, 0, 0, 36, 36, 3, 3, 17, 39, 3, 3, 17, 39, // 32..47
	1, 1, 16, 16, 1, 1, 37, 37, 2, 2, 18, 42, 2, 2, 40, 38, // 48..63
	12, 12, 24, 24, 12, 12, 24, 24, 5, 5, 19, 41, 5, 5, 19, 41, // 64..79
	4, 4, 6, 6, 4, 4, 30, 30, 7, 7, 46, 20, 7, 7, 8, 11, // 80..95
	12, 12, 24, 24, 12, 12, 24, 24, 15, 15, 43, 27, 15, 15, 43, 27, // 96..111
	4, 4, 6, 6, 4, 4, 30, 30, 29, 29, 21, 10, 29, 29, 34, 32, //112...127
	0, 0, 36, 36, 0, 0, 36, 36, 3, 3, 17, 39, 3, 3, 17, 39, // 128..143
	1, 1, 16, 16, 1, 1, 37, 37, 2, 2, 18, 42, 2, 2, 40, 38, // 144..159
	0, 0, 36, 36, 0, 0, 36, 36, 3, 3, 17, 39, 3, 3, 17, 39, // 160..175
	1, 1, 16, 16, 1, 1, 37, 37, 2, 2, 18, 42, 2, 2, 40, 38, // 176..191
	12, 12, 24, 24, 12, 12, 24, 24, 5, 5, 19, 41, 5, 5, 19, 41, // 192..207
	13, 13, 28, 28, 13, 13, 25, 25, 31, 31, 9, 35, 31, 31, 23, 33, // 208..223
	12, 12, 24, 24, 12, 12, 24, 24, 15, 15, 43, 27, 15, 15, 43, 27, // 224..239
	13, 13, 28, 28, 13, 13, 25, 25, 14, 14, 22, 44, 14, 14, 45, 26 // 240...255
};
// clang-format on

static const uint8_t g_case_index_to_connection_mask[BLOB9_TILE_COUNT] = {
	// - - -  case 0
	// - x -
	// - - -
	0b000'00'000,
	// - - -  case 1
	// - x o
	// - - -
	0b000'10'000,
	// - - -  case 2
	// o x o
	// - - -
	0b000'11'000,
	// - - -  case 3
	// o x -
	// - - -
	0b000'01'000,
	// - - -  case 4
	// - x o
	// - o -
	0b010'10'000,
	// - - -  case 5
	// o x -
	// - o -
	0b010'01'000,
	// - o -  case 6
	// - x o
	// - o -
	0b010'10'010,
	// - - -  case 7
	// o x o
	// - o -
	0b010'11'000,
	// - o o  case 8
	// o x o
	// - o -
	0b010'11'110,
	// - o -  case 9
	// o x o
	// - o o
	0b110'11'010,
	// o o -  case 10
	// o x o
	// o o -
	0b011'11'011,
	// o o o  case 11
	// o x o
	// - o -
	0b010'11'111,
	// - - -  case 12
	// - x -
	// - o -
	0b010'00'000,
	// - - -  case 13
	// - x o
	// - o o
	0b110'10'000,
	// - - -  case 14
	// o x o
	// o o o
	0b111'11'000,
	// - - -  case 15
	// o x -
	// o o -
	0b011'01'000,
	// - o -  case 16
	// - x o
	// - - -
	0b000'10'010,
	// - o -  case 17
	// o x -
	// - - -
	0b000'01'010,
	// - o -  case 18
	// o x o
	// - - -
	0b000'11'010,
	// - o -  case 19
	// o x -
	// - o -
	0b010'01'010,
	// o o -  case 20
	// o x o
	// - o -
	0b010'11'011,
	// - o -  case 21
	// o x o
	// o o -
	0b011'11'010,
	// - o -  case 22
	// o x o
	// o o o
	0b111'11'010,
	// - o o  case 23
	// o x o
	// - o o
	0b110'11'110,
	// - o -  case 24
	// - x -
	// - o -
	0b010'00'010,
	// - o o  case 25
	// - x o
	// - o o
	0b110'10'110,
	// o o o  case 26
	// o x o
	// o o o
	0b111'11'111,
	// o o -  case 27
	// o x -
	// o o -
	0b011'01'011,
	// - o -  case 28
	// - x o
	// - o o
	0b110'10'010,
	// - - -  case 29
	// o x o
	// o o -
	0b011'11'000,
	// - o o  case 30
	// - x o
	// - o -
	0b010'10'110,
	// - - -  case 31
	// o x o
	// - o o
	0b110'11'000,
	// o o o  case 32
	// o x o
	// o o -
	0b011'11'111,
	// o o o  case 33
	// o x o
	// - o o
	0b110'11'111,
	// - o o  case 34
	// o x o
	// o o -
	0b011'11'110,
	// o o -  case 35
	// o x o
	// - o o
	0b110'11'011,
	// - o -  case 36
	// - x -
	// - - -
	0b000'00'010,
	// - o o  case 37
	// - x o
	// - - -
	0b000'10'110,
	// o o o  case 38
	// o x o
	// - - -
	0b000'11'111,
	// o o -  case 39
	// o x -
	// - - -
	0b000'01'011,
	// - o o  case 40
	// o x o
	// - - -
	0b000'11'110,
	// o o -  case 41
	// o x -
	// - o -
	0b010'01'011,
	// o o -  case 42
	// o x o
	// - - -
	0b000'11'011,
	// - o -  case 43
	// o x -
	// o o -
	0b011'01'010,
	// o o -  case 44
	// o x o
	// o o o
	0b111'11'011,
	// - o o  case 45
	// o x o
	// o o o
	0b111'11'110,
	// - o -  case 46
	// o x o
	// - o -
	0b010'11'010
};

struct Compact5Op {
	uint8_t blob9_case_index;
	uint8_t compact5_case_index;
	// Tile sub-rectangle to copy across, in a coordinate system dividing the tile in 3x3 sub-tiles. Both points are
	// inclusive.
	uint8_t p0x;
	uint8_t p0y;
	uint8_t p1x;
	uint8_t p1y;
};

// List of image copy operations to perform in order to obtain all Blob9 tiles from 5 reference tiles.
// clang-format off
static const std::array<Compact5Op, 127> g_compact5_mapping_ops = {{
	// Case 0
	// - - -
	// - x -
	// - - -
	{ 0, COMPACT5_POINT, 0, 0, 2, 2 },

	// Case 1
	// - - -
	// - x o
	// - - -
	{ 1, COMPACT5_POINT, 0, 0, 0, 2 },
	{ 1, COMPACT5_HORIZONTAL, 1, 0, 2, 2 },

	// Case 2
	// - - -
	// o x o
	// - - -
	{ 2, COMPACT5_HORIZONTAL, 0, 0, 2, 2 },

	// Case 3
	// - - -
	// o x -
	// - - -
	{ 3, COMPACT5_HORIZONTAL, 0, 0, 1, 2 },
	{ 3, COMPACT5_POINT, 2, 0, 2, 2 },

	// Case 4
	// - - -
	// - x o
	// - o -
	{ 4, COMPACT5_CROSS, 1, 1, 2, 2 },
	{ 4, COMPACT5_VERTICAL, 0, 1, 0, 2 },
	{ 4, COMPACT5_HORIZONTAL, 1, 0, 2, 0 },
	{ 4, COMPACT5_POINT, 0, 0, 0, 0 },

	// Case 5
	// - - -
	// o x -
	// - o -
	{ 5, COMPACT5_CROSS, 0, 1, 1, 2 },
	{ 5, COMPACT5_VERTICAL, 2, 1, 2, 2 },
	{ 5, COMPACT5_HORIZONTAL, 0, 0, 1, 0 },
	{ 5, COMPACT5_POINT, 2, 0, 2, 0 },

	// Case 6
	// - o -
	// - x o
	// - o -
	{ 6, COMPACT5_VERTICAL, 0, 0, 1, 2 },
	{ 6, COMPACT5_CROSS, 2, 0, 2, 2 },

	// Case 7
	// - - -
	// o x o
	// - o -
	{ 7, COMPACT5_HORIZONTAL, 0, 0, 2, 0 },
	{ 7, COMPACT5_CROSS, 0, 1, 2, 2 },

	// Case 8
	// - o o
	// o x o
	// - o -
	{ 8, COMPACT5_CROSS, 0, 2, 2, 2 },
	{ 8, COMPACT5_CROSS, 0, 0, 0, 1 },
	{ 8, COMPACT5_FULL, 1, 0, 2, 1 },

	// Case 9
	// - o -
	// o x o
	// - o o
	{ 9, COMPACT5_CROSS, 0, 0, 2, 0 },
	{ 9, COMPACT5_CROSS, 0, 1, 0, 2 },
	{ 9, COMPACT5_FULL, 1, 1, 2, 2 },

	// Case 10
	// o o -
	// o x o
	// o o -
	{ 10, COMPACT5_FULL, 0, 0, 1, 2 },
	{ 10, COMPACT5_CROSS, 2, 0, 2, 2 },

	// Case 11
	// o o o
	// o x o
	// - o -
	{ 11, COMPACT5_FULL, 0, 0, 2, 1 },
	{ 11, COMPACT5_CROSS, 0, 2, 2, 2 },

	// Case 12
	// - - -
	// - x -
	// - o -
	{ 12, COMPACT5_POINT, 0, 0, 2, 0 },
	{ 12, COMPACT5_VERTICAL, 0, 1, 2, 2 },

	// Case 13
	// - - -
	// - x o
	// - o o
	{ 13, COMPACT5_POINT, 0, 0, 0, 0 },
	{ 13, COMPACT5_HORIZONTAL, 1, 0, 2, 0 },
	{ 13, COMPACT5_VERTICAL, 0, 1, 0, 2 },
	{ 13, COMPACT5_FULL, 1, 1, 2, 2 },

	// Case 14
	// - - -
	// o x o
	// o o o
	{ 14, COMPACT5_HORIZONTAL, 0, 0, 2, 0 },
	{ 14, COMPACT5_FULL, 0, 1, 2, 2 },

	// Case 15
	// - - -
	// o x -
	// o o -
	{ 15, COMPACT5_POINT, 2, 0, 2, 0 },
	{ 15, COMPACT5_HORIZONTAL, 0, 0, 1, 0 },
	{ 15, COMPACT5_VERTICAL, 2, 1, 2, 2 },
	{ 15, COMPACT5_FULL, 0, 1, 1, 2 },

	// Case 16
	// - o -
	// - x o
	// - - -
	{ 16, COMPACT5_CROSS, 1, 0, 2, 1 },
	{ 16, COMPACT5_VERTICAL, 0, 0, 0, 1 },
	{ 16, COMPACT5_HORIZONTAL, 1, 2, 2, 2 },
	{ 16, COMPACT5_POINT, 0, 2, 0, 2 },

	// Case 17
	// - o -
	// o x -
	// - - -
	{ 17, COMPACT5_CROSS, 0, 0, 1, 1 },
	{ 17, COMPACT5_VERTICAL, 2, 0, 2, 1 },
	{ 17, COMPACT5_HORIZONTAL, 0, 2, 1, 2 },
	{ 17, COMPACT5_POINT, 2, 2, 2, 2 },

	// Case 18
	// - o -
	// o x o
	// - - -
	{ 18, COMPACT5_CROSS, 0, 0, 2, 1 },
	{ 18, COMPACT5_HORIZONTAL, 0, 2, 2, 2 },

	// Case 19
	// - o -
	// o x -
	// - o -
	{ 19, COMPACT5_CROSS, 0, 0, 1, 2 },
	{ 19, COMPACT5_VERTICAL, 2, 0, 2, 2 },

	// Case 20
	// o o -
	// o x o
	// - o -
	{ 20, COMPACT5_FULL, 0, 0, 1, 1 },
	{ 20, COMPACT5_CROSS, 0, 2, 2, 2 },
	{ 20, COMPACT5_CROSS, 2, 0, 2, 1 },

	// Case 21
	// - o -
	// o x o
	// o o -
	{ 21, COMPACT5_FULL, 0, 1, 1, 2 },
	{ 21, COMPACT5_CROSS, 0, 0, 2, 0 },
	{ 21, COMPACT5_CROSS, 2, 1, 2, 2 },

	// Case 22
	// - o -
	// o x o
	// o o o
	{ 22, COMPACT5_FULL, 0, 1, 2, 2 },
	{ 22, COMPACT5_CROSS, 0, 0, 2, 0 },

	// Case 23
	// - o o
	// o x o
	// - o o
	{ 23, COMPACT5_FULL, 1, 0, 2, 2 },
	{ 23, COMPACT5_CROSS, 0, 0, 0, 2 },

	// Case 24
	// - o -
	// - x -
	// - o -
	{ 24, COMPACT5_VERTICAL, 0, 0, 2, 2 },

	// Case 25
	// - o o
	// - x o
	// - o o
	{ 25, COMPACT5_VERTICAL, 0, 0, 0, 2 },
	{ 25, COMPACT5_FULL, 1, 0, 2, 2 },

	// Case 26
	// o o o
	// o x o
	// o o o
	{ 26, COMPACT5_FULL, 0, 0, 2, 2 },

	// Case 27
	// o o -
	// o x -
	// o o -
	{ 27, COMPACT5_FULL, 0, 0, 1, 2 },
	{ 27, COMPACT5_VERTICAL, 2, 0, 2, 2 },

	// Case 28
	// - o -
	// - x o
	// - o o
	{ 28, COMPACT5_VERTICAL, 0, 0, 0, 2 },
	{ 28, COMPACT5_CROSS, 1, 0, 2, 0 },
	{ 28, COMPACT5_FULL, 1, 1, 2, 2 },

	// Case 29
	// - - -
	// o x o
	// o o -
	{ 29, COMPACT5_HORIZONTAL, 0, 0, 2, 0 },
	{ 29, COMPACT5_CROSS, 2, 1, 2, 2 },
	{ 29, COMPACT5_FULL, 0, 1, 1, 2 },

	// Case 30
	// - o o
	// - x o
	// - o -
	{ 30, COMPACT5_VERTICAL, 0, 0, 0, 2 },
	{ 30, COMPACT5_CROSS, 1, 2, 2, 2 },
	{ 30, COMPACT5_FULL, 1, 0, 2, 1 },

	// Case 31
	// - - -
	// o x o
	// - o o
	{ 31, COMPACT5_HORIZONTAL, 0, 0, 2, 0 },
	{ 31, COMPACT5_CROSS, 0, 1, 0, 2 },
	{ 31, COMPACT5_FULL, 1, 1, 2, 2 },

	// Case 32
	// o o o
	// o x o
	// o o -
	{ 32, COMPACT5_FULL, 0, 0, 2, 1 },
	{ 32, COMPACT5_FULL, 0, 2, 1, 2 },
	{ 32, COMPACT5_CROSS, 2, 2, 2, 2 },

	// Case 33
	// o o o
	// o x o
	// - o o
	{ 33, COMPACT5_FULL, 0, 0, 2, 1 },
	{ 33, COMPACT5_FULL, 1, 2, 2, 2 },
	{ 33, COMPACT5_CROSS, 0, 2, 0, 2 },

	// Case 34
	// - o o
	// o x o
	// o o -
	{ 34, COMPACT5_CROSS, 0, 0, 0, 0 },
	{ 34, COMPACT5_FULL, 1, 0, 2, 0 },
	{ 34, COMPACT5_FULL, 0, 1, 2, 1 },
	{ 34, COMPACT5_FULL, 0, 2, 1, 2 },
	{ 34, COMPACT5_CROSS, 2, 2, 2, 2 },

	// Case 35
	// o o -
	// o x o
	// - o o
	{ 35, COMPACT5_CROSS, 2, 0, 2, 0 },
	{ 35, COMPACT5_FULL, 0, 0, 1, 0 },
	{ 35, COMPACT5_FULL, 0, 1, 2, 1 },
	{ 35, COMPACT5_FULL, 1, 2, 2, 2 },
	{ 35, COMPACT5_CROSS, 0, 2, 0, 2 },

	// Case 36
	// - o -
	// - x -
	// - - -
	{ 36, COMPACT5_VERTICAL, 0, 0, 2, 1 },
	{ 36, COMPACT5_POINT, 0, 2, 2, 2 },

	// Case 37
	// - o o
	// - x o
	// - - -
	{ 37, COMPACT5_VERTICAL, 0, 0, 0, 1 },
	{ 37, COMPACT5_HORIZONTAL, 1, 2, 2, 2 },
	{ 37, COMPACT5_POINT, 0, 2, 0, 2 },
	{ 37, COMPACT5_FULL, 1, 0, 2, 1 },

	// Case 38
	// o o o
	// o x o
	// - - -
	{ 38, COMPACT5_FULL, 0, 0, 2, 1 },
	{ 38, COMPACT5_HORIZONTAL, 0, 2, 2, 2 },

	// Case 39
	// o o -
	// o x -
	// - - -
	{ 39, COMPACT5_FULL, 0, 0, 1, 1 },
	{ 39, COMPACT5_HORIZONTAL, 0, 2, 1, 2 },
	{ 39, COMPACT5_VERTICAL, 2, 0, 2, 1 },
	{ 39, COMPACT5_POINT, 2, 2, 2, 2 },

	// Case 40
	// - o o
	// o x o
	// - - -
	{ 40, COMPACT5_CROSS, 0, 0, 0, 1 },
	{ 40, COMPACT5_FULL, 1, 0, 2, 1 },
	{ 40, COMPACT5_HORIZONTAL, 0, 2, 2, 2 },

	// Case 41
	// o o -
	// o x -
	// - o -
	{ 41, COMPACT5_FULL, 0, 0, 1, 1 },
	{ 41, COMPACT5_VERTICAL, 2, 0, 2, 2 },
	{ 41, COMPACT5_CROSS, 0, 2, 1, 2 },

	// Case 42
	// o o -
	// o x o
	// - - -
	{ 42, COMPACT5_FULL, 0, 0, 1, 1 },
	{ 42, COMPACT5_CROSS, 2, 0, 2, 1 },
	{ 42, COMPACT5_HORIZONTAL, 0, 2, 2, 2 },

	// Case 43
	// - o -
	// o x -
	// o o -
	{ 43, COMPACT5_CROSS, 0, 0, 1, 0 },
	{ 43, COMPACT5_FULL, 0, 1, 1, 2 },
	{ 43, COMPACT5_VERTICAL, 2, 0, 2, 2 },

	// Case 44
	// o o -
	// o x o
	// o o o
	{ 44, COMPACT5_FULL, 0, 0, 1, 0 },
	{ 44, COMPACT5_CROSS, 2, 0, 2, 0 },
	{ 44, COMPACT5_FULL, 0, 1, 2, 2 },

	// Case 45
	// - o o
	// o x o
	// o o o
	{ 45, COMPACT5_CROSS, 0, 0, 0, 0 },
	{ 45, COMPACT5_FULL, 1, 0, 2, 0 },
	{ 45, COMPACT5_FULL, 0, 1, 2, 2 },

	// Case 46
	// - o -
	// o x o
	// - o -
	{ 46, COMPACT5_CROSS, 0, 0, 2, 2 }
}};
// clang-format on

uint8_t get_connection_mask_from_case_index(const uint8_t case_index) {
#ifdef DEV_ENABLED
	ZN_ASSERT(case_index >= 0 && case_index < BLOB9_TILE_COUNT);
#endif
	return g_case_index_to_connection_mask[case_index];
}

uint8_t get_case_index_from_connection_mask(const uint8_t cm) {
#ifdef DEV_ENABLED
	ZN_ASSERT(cm >= 0 && cm <= 255);
#endif
	return g_connection_mask_to_case_index[cm];
}

Vector2i get_layout_tile_coordinates_from_connection_mask(const uint8_t connection_mask) {
	const uint32_t case_index = get_case_index_from_connection_mask(connection_mask);
	return Vector2i(case_index % BLOB9_DEFAULT_LAYOUT_SIZE_X, case_index / BLOB9_DEFAULT_LAYOUT_SIZE_X);
}

// Compact5 tiles layout:
// - - -   - - -   - o -   - o -   o o o
// - x -   o x o   - x -   o x o   o x o
// - - -   - - -   - o -   - o -   o o o

std::array<uint8_t, 5> get_blob9_reference_cases_for_compact5() {
	std::array<uint8_t, 5> cases;
	cases[0] = 0;
	cases[1] = 2;
	cases[2] = 24;
	cases[3] = 46;
	cases[4] = 26;
	return cases;
}

void generate_atlas_from_compact5(
		const Image &input_image,
		const Vector2i tile_res,
		const std::array<Vector2i, 5> &input_positions,
		const Vector2i margin,
		Span<const Vector2i> output_positions,
		Image &output_image
) {
	const Vector2i output_image_size = tile_res * Vector2i(BLOB9_DEFAULT_LAYOUT_SIZE_X, BLOB9_DEFAULT_LAYOUT_SIZE_Y);

	const int xc_len = margin.x;
	const int yc_len = margin.y;

	const int xm_len = tile_res.x - 2 * xc_len;
	const int ym_len = tile_res.y - 2 * yc_len;

	const std::array<int, 4> x_offsets = { 0, xc_len, xc_len + xm_len, tile_res.x };
	const std::array<int, 4> y_offsets = { 0, yc_len, yc_len + ym_len, tile_res.y };

	for (unsigned int op_index = 0; op_index < g_compact5_mapping_ops.size(); ++op_index) {
		const Compact5Op op = g_compact5_mapping_ops[op_index];

		const Vector2i p0(x_offsets[op.p0x + 0], y_offsets[op.p0y + 0]);
		const Vector2i p1(x_offsets[op.p1x + 1], y_offsets[op.p1y + 1]);

		const Rect2i src_rect(input_positions[op.compact5_case_index] + p0, p1 - p0);

		const Vector2i dst_tile_origin = output_positions[op.blob9_case_index];
		const Vector2i dst_pos = dst_tile_origin + p0;

		output_image.blit_rect(Ref<Image>(&input_image), src_rect, dst_pos);
	}
}

void generate_atlas_from_compact5(
		const Image &input_image,
		const Vector2i tile_res,
		const std::array<Vector2i, 5> &input_positions,
		const Vector2i margin,
		Image &output_image,
		const Vector2i output_position
) {
	std::array<Vector2i, BLOB9_TILE_COUNT> output_positions;
	for (unsigned int i = 0; i < BLOB9_TILE_COUNT; ++i) {
		output_positions[i] = g_default_layout_tile_positions[i] * tile_res + output_position;
	}
	generate_atlas_from_compact5(
			input_image, tile_res, input_positions, margin, to_span(output_positions), output_image
	);
}

} // namespace zylann::voxel::blocky
