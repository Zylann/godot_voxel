#include "voxel_mesher_dmc.h"
#include "../storage/voxel_buffer.h"
#include "../thirdparty/robin_hood/robin_hood.h"
#include "../util/containers/fixed_array.h"
#include "../util/containers/span.h"
#include "../util/containers/std_unordered_map.h"
#include "../util/containers/std_vector.h"
#include "../util/godot/core/packed_arrays.h"
#include "../util/math/conv.h"
#include "../util/math/vector3f.h"
#include "../util/math/vector3i.h"
#include "../util/math/vector3u32.h"
#include "../util/profiling.h"
#include <cstdint>

namespace zylann {
namespace voxel {
namespace dmc {

// This implementation of dual marching cubes doesn't do adaptative simplification (no octree), but is fast, simple and
// generates nicer geometry than marching cubes (no slivers). LOD may be done by polygonizing 1 extra voxel over, at the
// cost of a little more data fetching and overdraw.

// Based on original implementation from FastNoise2:
//
// MIT License
//
// Copyright (c) 2020 Jordan Peck
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

namespace tables {

enum EdgeCode : uint16_t {
	EDGE0 = 1,
	EDGE1 = 1 << 1,
	EDGE2 = 1 << 2,
	EDGE3 = 1 << 3,
	EDGE4 = 1 << 4,
	EDGE5 = 1 << 5,
	EDGE6 = 1 << 6,
	EDGE7 = 1 << 7,
	EDGE8 = 1 << 8,
	EDGE9 = 1 << 9,
	EDGE10 = 1 << 10,
	EDGE11 = 1 << 11,
};

//  Coordinate system
//
//       y
//       |
//       |
//       |
//       0-----x
//      /
//     /
//    z
//

// Cell Corners
// (Corners are voxels. Number correspond to Morton codes of corner coordinates)
//
//       2-------------------3
//      /|                  /|
//     / |                 / |
//    /  |                /  |
//   6-------------------7   |
//   |   |               |   |
//   |   |               |   |
//   |   |               |   |
//   |   |               |   |
//   |   0---------------|---1
//   |  /                |  /
//   | /                 | /
//   |/                  |/
//   4-------------------5
//

//         Cell Edges
//
//       o--------4----------o
//      /|                  /|
//     7 |                 5 |
//    /  |                /  |
//   o--------6----------o   |
//   |   8               |   9
//   |   |               |   |
//   |   |               |   |
//   11  |               10  |
//   |   o--------0------|---o
//   |  /                |  /
//   | 3                 | 1
//   |/                  |/
//   o--------2----------o
//

// Encodes the edge vertices for the 256 marching cubes cases.
// A marching cube case produces up to four faces and, thus, up to four
// dual points.

const uint16_t dual_points_list[256][4] = {
	{ 0, 0, 0, 0 }, // 0
	{ EDGE0 | EDGE3 | EDGE8, 0, 0, 0 }, // 1
	{ EDGE0 | EDGE1 | EDGE9, 0, 0, 0 }, // 2
	{ EDGE1 | EDGE3 | EDGE8 | EDGE9, 0, 0, 0 }, // 3
	{ EDGE4 | EDGE7 | EDGE8, 0, 0, 0 }, // 4
	{ EDGE0 | EDGE3 | EDGE4 | EDGE7, 0, 0, 0 }, // 5
	{ EDGE0 | EDGE1 | EDGE9, EDGE4 | EDGE7 | EDGE8, 0, 0 }, // 6
	{ EDGE1 | EDGE3 | EDGE4 | EDGE7 | EDGE9, 0, 0, 0 }, // 7
	{ EDGE4 | EDGE5 | EDGE9, 0, 0, 0 }, // 8
	{ EDGE0 | EDGE3 | EDGE8, EDGE4 | EDGE5 | EDGE9, 0, 0 }, // 9
	{ EDGE0 | EDGE1 | EDGE4 | EDGE5, 0, 0, 0 }, // 10
	{ EDGE1 | EDGE3 | EDGE4 | EDGE5 | EDGE8, 0, 0, 0 }, // 11
	{ EDGE5 | EDGE7 | EDGE8 | EDGE9, 0, 0, 0 }, // 12
	{ EDGE0 | EDGE3 | EDGE5 | EDGE7 | EDGE9, 0, 0, 0 }, // 13
	{ EDGE0 | EDGE1 | EDGE5 | EDGE7 | EDGE8, 0, 0, 0 }, // 14
	{ EDGE1 | EDGE3 | EDGE5 | EDGE7, 0, 0, 0 }, // 15
	{ EDGE2 | EDGE3 | EDGE11, 0, 0, 0 }, // 16
	{ EDGE0 | EDGE2 | EDGE8 | EDGE11, 0, 0, 0 }, // 17
	{ EDGE0 | EDGE1 | EDGE9, EDGE2 | EDGE3 | EDGE11, 0, 0 }, // 18
	{ EDGE1 | EDGE2 | EDGE8 | EDGE9 | EDGE11, 0, 0, 0 }, // 19
	{ EDGE4 | EDGE7 | EDGE8, EDGE2 | EDGE3 | EDGE11, 0, 0 }, // 20
	{ EDGE0 | EDGE2 | EDGE4 | EDGE7 | EDGE11, 0, 0, 0 }, // 21
	{ EDGE0 | EDGE1 | EDGE9, EDGE4 | EDGE7 | EDGE8, EDGE2 | EDGE3 | EDGE11, 0 }, // 22
	{ EDGE1 | EDGE2 | EDGE4 | EDGE7 | EDGE9 | EDGE11, 0, 0, 0 }, // 23
	{ EDGE4 | EDGE5 | EDGE9, EDGE2 | EDGE3 | EDGE11, 0, 0 }, // 24
	{ EDGE0 | EDGE2 | EDGE8 | EDGE11, EDGE4 | EDGE5 | EDGE9, 0, 0 }, // 25
	{ EDGE0 | EDGE1 | EDGE4 | EDGE5, EDGE2 | EDGE3 | EDGE11, 0, 0 }, // 26
	{ EDGE1 | EDGE2 | EDGE4 | EDGE5 | EDGE8 | EDGE11, 0, 0, 0 }, // 27
	{ EDGE5 | EDGE7 | EDGE8 | EDGE9, EDGE2 | EDGE3 | EDGE11, 0, 0 }, // 28
	{ EDGE0 | EDGE2 | EDGE5 | EDGE7 | EDGE9 | EDGE11, 0, 0, 0 }, // 29
	{ EDGE0 | EDGE1 | EDGE5 | EDGE7 | EDGE8, EDGE2 | EDGE3 | EDGE11, 0, 0 }, // 30
	{ EDGE1 | EDGE2 | EDGE5 | EDGE7 | EDGE11, 0, 0, 0 }, // 31
	{ EDGE1 | EDGE2 | EDGE10, 0, 0, 0 }, // 32
	{ EDGE0 | EDGE3 | EDGE8, EDGE1 | EDGE2 | EDGE10, 0, 0 }, // 33
	{ EDGE0 | EDGE2 | EDGE9 | EDGE10, 0, 0, 0 }, // 34
	{ EDGE2 | EDGE3 | EDGE8 | EDGE9 | EDGE10, 0, 0, 0 }, // 35
	{ EDGE4 | EDGE7 | EDGE8, EDGE1 | EDGE2 | EDGE10, 0, 0 }, // 36
	{ EDGE0 | EDGE3 | EDGE4 | EDGE7, EDGE1 | EDGE2 | EDGE10, 0, 0 }, // 37
	{ EDGE0 | EDGE2 | EDGE9 | EDGE10, EDGE4 | EDGE7 | EDGE8, 0, 0 }, // 38
	{ EDGE2 | EDGE3 | EDGE4 | EDGE7 | EDGE9 | EDGE10, 0, 0, 0 }, // 39
	{ EDGE4 | EDGE5 | EDGE9, EDGE1 | EDGE2 | EDGE10, 0, 0 }, // 40
	{ EDGE0 | EDGE3 | EDGE8, EDGE4 | EDGE5 | EDGE9, EDGE1 | EDGE2 | EDGE10, 0 }, // 41
	{ EDGE0 | EDGE2 | EDGE4 | EDGE5 | EDGE10, 0, 0, 0 }, // 42
	{ EDGE2 | EDGE3 | EDGE4 | EDGE5 | EDGE8 | EDGE10, 0, 0, 0 }, // 43
	{ EDGE5 | EDGE7 | EDGE8 | EDGE9, EDGE1 | EDGE2 | EDGE10, 0, 0 }, // 44
	{ EDGE0 | EDGE3 | EDGE5 | EDGE7 | EDGE9, EDGE1 | EDGE2 | EDGE10, 0, 0 }, // 45
	{ EDGE0 | EDGE2 | EDGE5 | EDGE7 | EDGE8 | EDGE10, 0, 0, 0 }, // 46
	{ EDGE2 | EDGE3 | EDGE5 | EDGE7 | EDGE10, 0, 0, 0 }, // 47
	{ EDGE1 | EDGE3 | EDGE10 | EDGE11, 0, 0, 0 }, // 48
	{ EDGE0 | EDGE1 | EDGE8 | EDGE10 | EDGE11, 0, 0, 0 }, // 49
	{ EDGE0 | EDGE3 | EDGE9 | EDGE10 | EDGE11, 0, 0, 0 }, // 50
	{ EDGE8 | EDGE9 | EDGE10 | EDGE11, 0, 0, 0 }, // 51
	{ EDGE4 | EDGE7 | EDGE8, EDGE1 | EDGE3 | EDGE10 | EDGE11, 0, 0 }, // 52
	{ EDGE0 | EDGE1 | EDGE4 | EDGE7 | EDGE10 | EDGE11, 0, 0, 0 }, // 53
	{ EDGE0 | EDGE3 | EDGE9 | EDGE10 | EDGE11, EDGE4 | EDGE7 | EDGE8, 0, 0 }, // 54
	{ EDGE4 | EDGE7 | EDGE9 | EDGE10 | EDGE11, 0, 0, 0 }, // 55
	{ EDGE4 | EDGE5 | EDGE9, EDGE1 | EDGE3 | EDGE10 | EDGE11, 0, 0 }, // 56
	{ EDGE0 | EDGE1 | EDGE8 | EDGE10 | EDGE11, EDGE4 | EDGE5 | EDGE9, 0, 0 }, // 57
	{ EDGE0 | EDGE3 | EDGE4 | EDGE5 | EDGE10 | EDGE11, 0, 0, 0 }, // 58
	{ EDGE4 | EDGE5 | EDGE8 | EDGE10 | EDGE11, 0, 0, 0 }, // 59
	{ EDGE5 | EDGE7 | EDGE8 | EDGE9, EDGE1 | EDGE3 | EDGE10 | EDGE11, 0, 0 }, // 60
	{ EDGE0 | EDGE1 | EDGE5 | EDGE7 | EDGE9 | EDGE10 | EDGE11, 0, 0, 0 }, // 61
	{ EDGE0 | EDGE3 | EDGE5 | EDGE7 | EDGE8 | EDGE10 | EDGE11, 0, 0, 0 }, // 62
	{ EDGE5 | EDGE7 | EDGE10 | EDGE11, 0, 0, 0 }, // 63
	{ EDGE6 | EDGE7 | EDGE11, 0, 0, 0 }, // 64
	{ EDGE0 | EDGE3 | EDGE8, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 65
	{ EDGE0 | EDGE1 | EDGE9, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 66
	{ EDGE1 | EDGE3 | EDGE8 | EDGE9, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 67
	{ EDGE4 | EDGE6 | EDGE8 | EDGE11, 0, 0, 0 }, // 68
	{ EDGE0 | EDGE3 | EDGE4 | EDGE6 | EDGE11, 0, 0, 0 }, // 69
	{ EDGE0 | EDGE1 | EDGE9, EDGE4 | EDGE6 | EDGE8 | EDGE11, 0, 0 }, // 70
	{ EDGE1 | EDGE3 | EDGE4 | EDGE6 | EDGE9 | EDGE11, 0, 0, 0 }, // 71
	{ EDGE4 | EDGE5 | EDGE9, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 72
	{ EDGE0 | EDGE3 | EDGE8, EDGE4 | EDGE5 | EDGE9, EDGE6 | EDGE7 | EDGE11, 0 }, // 73
	{ EDGE0 | EDGE1 | EDGE4 | EDGE5, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 74
	{ EDGE1 | EDGE3 | EDGE4 | EDGE5 | EDGE8, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 75
	{ EDGE5 | EDGE6 | EDGE8 | EDGE9 | EDGE11, 0, 0, 0 }, // 76
	{ EDGE0 | EDGE3 | EDGE5 | EDGE6 | EDGE9 | EDGE11, 0, 0, 0 }, // 77
	{ EDGE0 | EDGE1 | EDGE5 | EDGE6 | EDGE8 | EDGE11, 0, 0, 0 }, // 78
	{ EDGE1 | EDGE3 | EDGE5 | EDGE6 | EDGE11, 0, 0, 0 }, // 79
	{ EDGE2 | EDGE3 | EDGE6 | EDGE7, 0, 0, 0 }, // 80
	{ EDGE0 | EDGE2 | EDGE6 | EDGE7 | EDGE8, 0, 0, 0 }, // 81
	{ EDGE0 | EDGE1 | EDGE9, EDGE2 | EDGE3 | EDGE6 | EDGE7, 0, 0 }, // 82
	{ EDGE1 | EDGE2 | EDGE6 | EDGE7 | EDGE8 | EDGE9, 0, 0, 0 }, // 83
	{ EDGE2 | EDGE3 | EDGE4 | EDGE6 | EDGE8, 0, 0, 0 }, // 84
	{ EDGE0 | EDGE2 | EDGE4 | EDGE6, 0, 0, 0 }, // 85
	{ EDGE0 | EDGE1 | EDGE9, EDGE2 | EDGE3 | EDGE4 | EDGE6 | EDGE8, 0, 0 }, // 86
	{ EDGE1 | EDGE2 | EDGE4 | EDGE6 | EDGE9, 0, 0, 0 }, // 87
	{ EDGE4 | EDGE5 | EDGE9, EDGE2 | EDGE3 | EDGE6 | EDGE7, 0, 0 }, // 88
	{ EDGE0 | EDGE2 | EDGE6 | EDGE7 | EDGE8, EDGE4 | EDGE5 | EDGE9, 0, 0 }, // 89
	{ EDGE0 | EDGE1 | EDGE4 | EDGE5, EDGE2 | EDGE3 | EDGE6 | EDGE7, 0, 0 }, // 90
	{ EDGE1 | EDGE2 | EDGE4 | EDGE5 | EDGE6 | EDGE7 | EDGE8, 0, 0, 0 }, // 91
	{ EDGE2 | EDGE3 | EDGE5 | EDGE6 | EDGE8 | EDGE9, 0, 0, 0 }, // 92
	{ EDGE0 | EDGE2 | EDGE5 | EDGE6 | EDGE9, 0, 0, 0 }, // 93
	{ EDGE0 | EDGE1 | EDGE2 | EDGE3 | EDGE5 | EDGE6 | EDGE8, 0, 0, 0 }, // 94
	{ EDGE1 | EDGE2 | EDGE5 | EDGE6, 0, 0, 0 }, // 95
	{ EDGE1 | EDGE2 | EDGE10, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 96
	{ EDGE0 | EDGE3 | EDGE8, EDGE1 | EDGE2 | EDGE10, EDGE6 | EDGE7 | EDGE11, 0 }, // 97
	{ EDGE0 | EDGE2 | EDGE9 | EDGE10, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 98
	{ EDGE2 | EDGE3 | EDGE8 | EDGE9 | EDGE10, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 99
	{ EDGE4 | EDGE6 | EDGE8 | EDGE11, EDGE1 | EDGE2 | EDGE10, 0, 0 }, // 100
	{ EDGE0 | EDGE3 | EDGE4 | EDGE6 | EDGE11, EDGE1 | EDGE2 | EDGE10, 0, 0 }, // 101
	{ EDGE0 | EDGE2 | EDGE9 | EDGE10, EDGE4 | EDGE6 | EDGE8 | EDGE11, 0, 0 }, // 102
	{ EDGE2 | EDGE3 | EDGE4 | EDGE6 | EDGE9 | EDGE10 | EDGE11, 0, 0, 0 }, // 103
	{ EDGE4 | EDGE5 | EDGE9, EDGE1 | EDGE2 | EDGE10, EDGE6 | EDGE7 | EDGE11, 0 }, // 104
	{ EDGE0 | EDGE3 | EDGE8, EDGE4 | EDGE5 | EDGE9, EDGE1 | EDGE2 | EDGE10, EDGE6 | EDGE7 | EDGE11 }, // 105
	{ EDGE0 | EDGE2 | EDGE4 | EDGE5 | EDGE10, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 106
	{ EDGE2 | EDGE3 | EDGE4 | EDGE5 | EDGE8 | EDGE10, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 107
	{ EDGE5 | EDGE6 | EDGE8 | EDGE9 | EDGE11, EDGE1 | EDGE2 | EDGE10, 0, 0 }, // 108
	{ EDGE0 | EDGE3 | EDGE5 | EDGE6 | EDGE9 | EDGE11, EDGE1 | EDGE2 | EDGE10, 0, 0 }, // 109
	{ EDGE0 | EDGE2 | EDGE5 | EDGE6 | EDGE8 | EDGE10 | EDGE11, 0, 0, 0 }, // 110
	{ EDGE2 | EDGE3 | EDGE5 | EDGE6 | EDGE10 | EDGE11, 0, 0, 0 }, // 111
	{ EDGE1 | EDGE3 | EDGE6 | EDGE7 | EDGE10, 0, 0, 0 }, // 112
	{ EDGE0 | EDGE1 | EDGE6 | EDGE7 | EDGE8 | EDGE10, 0, 0, 0 }, // 113
	{ EDGE0 | EDGE3 | EDGE6 | EDGE7 | EDGE9 | EDGE10, 0, 0, 0 }, // 114
	{ EDGE6 | EDGE7 | EDGE8 | EDGE9 | EDGE10, 0, 0, 0 }, // 115
	{ EDGE1 | EDGE3 | EDGE4 | EDGE6 | EDGE8 | EDGE10, 0, 0, 0 }, // 116
	{ EDGE0 | EDGE1 | EDGE4 | EDGE6 | EDGE10, 0, 0, 0 }, // 117
	{ EDGE0 | EDGE3 | EDGE4 | EDGE6 | EDGE8 | EDGE9 | EDGE10, 0, 0, 0 }, // 118
	{ EDGE4 | EDGE6 | EDGE9 | EDGE10, 0, 0, 0 }, // 119
	{ EDGE4 | EDGE5 | EDGE9, EDGE1 | EDGE3 | EDGE6 | EDGE7 | EDGE10, 0, 0 }, // 120
	{ EDGE0 | EDGE1 | EDGE6 | EDGE7 | EDGE8 | EDGE10, EDGE4 | EDGE5 | EDGE9, 0, 0 }, // 121
	{ EDGE0 | EDGE3 | EDGE4 | EDGE5 | EDGE6 | EDGE7 | EDGE10, 0, 0, 0 }, // 122
	{ EDGE4 | EDGE5 | EDGE6 | EDGE7 | EDGE8 | EDGE10, 0, 0, 0 }, // 123
	{ EDGE1 | EDGE3 | EDGE5 | EDGE6 | EDGE8 | EDGE9 | EDGE10, 0, 0, 0 }, // 124
	{ EDGE0 | EDGE1 | EDGE5 | EDGE6 | EDGE9 | EDGE10, 0, 0, 0 }, // 125
	{ EDGE0 | EDGE3 | EDGE8, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 126
	{ EDGE5 | EDGE6 | EDGE10, 0, 0, 0 }, // 127
	{ EDGE5 | EDGE6 | EDGE10, 0, 0, 0 }, // 128
	{ EDGE0 | EDGE3 | EDGE8, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 129
	{ EDGE0 | EDGE1 | EDGE9, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 130
	{ EDGE1 | EDGE3 | EDGE8 | EDGE9, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 131
	{ EDGE4 | EDGE7 | EDGE8, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 132
	{ EDGE0 | EDGE3 | EDGE4 | EDGE7, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 133
	{ EDGE0 | EDGE1 | EDGE9, EDGE4 | EDGE7 | EDGE8, EDGE5 | EDGE6 | EDGE10, 0 }, // 134
	{ EDGE1 | EDGE3 | EDGE4 | EDGE7 | EDGE9, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 135
	{ EDGE4 | EDGE6 | EDGE9 | EDGE10, 0, 0, 0 }, // 136
	{ EDGE0 | EDGE3 | EDGE8, EDGE4 | EDGE6 | EDGE9 | EDGE10, 0, 0 }, // 137
	{ EDGE0 | EDGE1 | EDGE4 | EDGE6 | EDGE10, 0, 0, 0 }, // 138
	{ EDGE1 | EDGE3 | EDGE4 | EDGE6 | EDGE8 | EDGE10, 0, 0, 0 }, // 139
	{ EDGE6 | EDGE7 | EDGE8 | EDGE9 | EDGE10, 0, 0, 0 }, // 140
	{ EDGE0 | EDGE3 | EDGE6 | EDGE7 | EDGE9 | EDGE10, 0, 0, 0 }, // 141
	{ EDGE0 | EDGE1 | EDGE6 | EDGE7 | EDGE8 | EDGE10, 0, 0, 0 }, // 142
	{ EDGE1 | EDGE3 | EDGE6 | EDGE7 | EDGE10, 0, 0, 0 }, // 143
	{ EDGE2 | EDGE3 | EDGE11, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 144
	{ EDGE0 | EDGE2 | EDGE8 | EDGE11, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 145
	{ EDGE0 | EDGE1 | EDGE9, EDGE2 | EDGE3 | EDGE11, EDGE5 | EDGE6 | EDGE10, 0 }, // 146
	{ EDGE1 | EDGE2 | EDGE8 | EDGE9 | EDGE11, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 147
	{ EDGE4 | EDGE7 | EDGE8, EDGE2 | EDGE3 | EDGE11, EDGE5 | EDGE6 | EDGE10, 0 }, // 148
	{ EDGE0 | EDGE2 | EDGE4 | EDGE7 | EDGE11, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 149
	{ EDGE0 | EDGE1 | EDGE9, EDGE4 | EDGE7 | EDGE8, EDGE2 | EDGE3 | EDGE11, EDGE5 | EDGE6 | EDGE10 }, // 150
	{ EDGE1 | EDGE2 | EDGE4 | EDGE7 | EDGE9 | EDGE11, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 151
	{ EDGE4 | EDGE6 | EDGE9 | EDGE10, EDGE2 | EDGE3 | EDGE11, 0, 0 }, // 152
	{ EDGE0 | EDGE2 | EDGE8 | EDGE11, EDGE4 | EDGE6 | EDGE9 | EDGE10, 0, 0 }, // 153
	{ EDGE0 | EDGE1 | EDGE4 | EDGE6 | EDGE10, EDGE2 | EDGE3 | EDGE11, 0, 0 }, // 154
	{ EDGE1 | EDGE2 | EDGE4 | EDGE6 | EDGE8 | EDGE10 | EDGE11, 0, 0, 0 }, // 155
	{ EDGE6 | EDGE7 | EDGE8 | EDGE9 | EDGE10, EDGE2 | EDGE3 | EDGE11, 0, 0 }, // 156
	{ EDGE0 | EDGE2 | EDGE6 | EDGE7 | EDGE9 | EDGE10 | EDGE11, 0, 0, 0 }, // 157
	{ EDGE0 | EDGE1 | EDGE6 | EDGE7 | EDGE8 | EDGE10, EDGE2 | EDGE3 | EDGE11, 0, 0 }, // 158
	{ EDGE1 | EDGE2 | EDGE6 | EDGE7 | EDGE10 | EDGE11, 0, 0, 0 }, // 159
	{ EDGE1 | EDGE2 | EDGE5 | EDGE6, 0, 0, 0 }, // 160
	{ EDGE0 | EDGE3 | EDGE8, EDGE1 | EDGE2 | EDGE5 | EDGE6, 0, 0 }, // 161
	{ EDGE0 | EDGE2 | EDGE5 | EDGE6 | EDGE9, 0, 0, 0 }, // 162
	{ EDGE2 | EDGE3 | EDGE5 | EDGE6 | EDGE8 | EDGE9, 0, 0, 0 }, // 163
	{ EDGE4 | EDGE7 | EDGE8, EDGE1 | EDGE2 | EDGE5 | EDGE6, 0, 0 }, // 164
	{ EDGE0 | EDGE3 | EDGE4 | EDGE7, EDGE1 | EDGE2 | EDGE5 | EDGE6, 0, 0 }, // 165
	{ EDGE0 | EDGE2 | EDGE5 | EDGE6 | EDGE9, EDGE4 | EDGE7 | EDGE8, 0, 0 }, // 166
	{ EDGE2 | EDGE3 | EDGE4 | EDGE5 | EDGE6 | EDGE7 | EDGE9, 0, 0, 0 }, // 167
	{ EDGE1 | EDGE2 | EDGE4 | EDGE6 | EDGE9, 0, 0, 0 }, // 168
	{ EDGE0 | EDGE3 | EDGE8, EDGE1 | EDGE2 | EDGE4 | EDGE6 | EDGE9, 0, 0 }, // 169
	{ EDGE0 | EDGE2 | EDGE4 | EDGE6, 0, 0, 0 }, // 170
	{ EDGE2 | EDGE3 | EDGE4 | EDGE6 | EDGE8, 0, 0, 0 }, // 171
	{ EDGE1 | EDGE2 | EDGE6 | EDGE7 | EDGE8 | EDGE9, 0, 0, 0 }, // 172
	{ EDGE0 | EDGE1 | EDGE2 | EDGE3 | EDGE6 | EDGE7 | EDGE9, 0, 0, 0 }, // 173
	{ EDGE0 | EDGE2 | EDGE6 | EDGE7 | EDGE8, 0, 0, 0 }, // 174
	{ EDGE2 | EDGE3 | EDGE6 | EDGE7, 0, 0, 0 }, // 175
	{ EDGE1 | EDGE3 | EDGE5 | EDGE6 | EDGE11, 0, 0, 0 }, // 176
	{ EDGE0 | EDGE1 | EDGE5 | EDGE6 | EDGE8 | EDGE11, 0, 0, 0 }, // 177
	{ EDGE0 | EDGE3 | EDGE5 | EDGE6 | EDGE9 | EDGE11, 0, 0, 0 }, // 178
	{ EDGE5 | EDGE6 | EDGE8 | EDGE9 | EDGE11, 0, 0, 0 }, // 179
	{ EDGE4 | EDGE7 | EDGE8, EDGE1 | EDGE3 | EDGE5 | EDGE6 | EDGE11, 0, 0 }, // 180
	{ EDGE0 | EDGE1 | EDGE4 | EDGE5 | EDGE6 | EDGE7 | EDGE11, 0, 0, 0 }, // 181
	{ EDGE0 | EDGE3 | EDGE5 | EDGE6 | EDGE9 | EDGE11, EDGE4 | EDGE7 | EDGE8, 0, 0 }, // 182
	{ EDGE4 | EDGE5 | EDGE6 | EDGE7 | EDGE9 | EDGE11, 0, 0, 0 }, // 183
	{ EDGE1 | EDGE3 | EDGE4 | EDGE6 | EDGE9 | EDGE11, 0, 0, 0 }, // 184
	{ EDGE0 | EDGE1 | EDGE4 | EDGE6 | EDGE8 | EDGE9 | EDGE11, 0, 0, 0 }, // 185
	{ EDGE0 | EDGE3 | EDGE4 | EDGE6 | EDGE11, 0, 0, 0 }, // 186
	{ EDGE4 | EDGE6 | EDGE8 | EDGE11, 0, 0, 0 }, // 187
	{ EDGE1 | EDGE3 | EDGE6 | EDGE7 | EDGE8 | EDGE9 | EDGE11, 0, 0, 0 }, // 188
	{ EDGE0 | EDGE1 | EDGE9, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 189
	{ EDGE0 | EDGE3 | EDGE6 | EDGE7 | EDGE8 | EDGE11, 0, 0, 0 }, // 190
	{ EDGE6 | EDGE7 | EDGE11, 0, 0, 0 }, // 191
	{ EDGE5 | EDGE7 | EDGE10 | EDGE11, 0, 0, 0 }, // 192
	{ EDGE0 | EDGE3 | EDGE8, EDGE5 | EDGE7 | EDGE10 | EDGE11, 0, 0 }, // 193
	{ EDGE0 | EDGE1 | EDGE9, EDGE5 | EDGE7 | EDGE10 | EDGE11, 0, 0 }, // 194
	{ EDGE1 | EDGE3 | EDGE8 | EDGE9, EDGE5 | EDGE7 | EDGE10 | EDGE11, 0, 0 }, // 195
	{ EDGE4 | EDGE5 | EDGE8 | EDGE10 | EDGE11, 0, 0, 0 }, // 196
	{ EDGE0 | EDGE3 | EDGE4 | EDGE5 | EDGE10 | EDGE11, 0, 0, 0 }, // 197
	{ EDGE0 | EDGE1 | EDGE9, EDGE4 | EDGE5 | EDGE8 | EDGE10 | EDGE11, 0, 0 }, // 198
	{ EDGE1 | EDGE3 | EDGE4 | EDGE5 | EDGE9 | EDGE10 | EDGE11, 0, 0, 0 }, // 199
	{ EDGE4 | EDGE7 | EDGE9 | EDGE10 | EDGE11, 0, 0, 0 }, // 200
	{ EDGE0 | EDGE3 | EDGE8, EDGE4 | EDGE7 | EDGE9 | EDGE10 | EDGE11, 0, 0 }, // 201
	{ EDGE0 | EDGE1 | EDGE4 | EDGE7 | EDGE10 | EDGE11, 0, 0, 0 }, // 202
	{ EDGE1 | EDGE3 | EDGE4 | EDGE7 | EDGE8 | EDGE10 | EDGE11, 0, 0, 0 }, // 203
	{ EDGE8 | EDGE9 | EDGE10 | EDGE11, 0, 0, 0 }, // 204
	{ EDGE0 | EDGE3 | EDGE9 | EDGE10 | EDGE11, 0, 0, 0 }, // 205
	{ EDGE0 | EDGE1 | EDGE8 | EDGE10 | EDGE11, 0, 0, 0 }, // 206
	{ EDGE1 | EDGE3 | EDGE10 | EDGE11, 0, 0, 0 }, // 207
	{ EDGE2 | EDGE3 | EDGE5 | EDGE7 | EDGE10, 0, 0, 0 }, // 208
	{ EDGE0 | EDGE2 | EDGE5 | EDGE7 | EDGE8 | EDGE10, 0, 0, 0 }, // 209
	{ EDGE0 | EDGE1 | EDGE9, EDGE2 | EDGE3 | EDGE5 | EDGE7 | EDGE10, 0, 0 }, // 210
	{ EDGE1 | EDGE2 | EDGE5 | EDGE7 | EDGE8 | EDGE9 | EDGE10, 0, 0, 0 }, // 211
	{ EDGE2 | EDGE3 | EDGE4 | EDGE5 | EDGE8 | EDGE10, 0, 0, 0 }, // 212
	{ EDGE0 | EDGE2 | EDGE4 | EDGE5 | EDGE10, 0, 0, 0 }, // 213
	{ EDGE0 | EDGE1 | EDGE9, EDGE2 | EDGE3 | EDGE4 | EDGE5 | EDGE8 | EDGE10, 0, 0 }, // 214
	{ EDGE1 | EDGE2 | EDGE4 | EDGE5 | EDGE9 | EDGE10, 0, 0, 0 }, // 215
	{ EDGE2 | EDGE3 | EDGE4 | EDGE7 | EDGE9 | EDGE10, 0, 0, 0 }, // 216
	{ EDGE0 | EDGE2 | EDGE4 | EDGE7 | EDGE8 | EDGE9 | EDGE10, 0, 0, 0 }, // 217
	{ EDGE0 | EDGE1 | EDGE2 | EDGE3 | EDGE4 | EDGE7 | EDGE10, 0, 0, 0 }, // 218
	{ EDGE4 | EDGE7 | EDGE8, EDGE1 | EDGE2 | EDGE10, 0, 0 }, // 219
	{ EDGE2 | EDGE3 | EDGE8 | EDGE9 | EDGE10, 0, 0, 0 }, // 220
	{ EDGE0 | EDGE2 | EDGE9 | EDGE10, 0, 0, 0 }, // 221
	{ EDGE0 | EDGE1 | EDGE2 | EDGE3 | EDGE8 | EDGE10, 0, 0, 0 }, // 222
	{ EDGE1 | EDGE2 | EDGE10, 0, 0, 0 }, // 223
	{ EDGE1 | EDGE2 | EDGE5 | EDGE7 | EDGE11, 0, 0, 0 }, // 224
	{ EDGE0 | EDGE3 | EDGE8, EDGE1 | EDGE2 | EDGE5 | EDGE7 | EDGE11, 0, 0 }, // 225
	{ EDGE0 | EDGE2 | EDGE5 | EDGE7 | EDGE9 | EDGE11, 0, 0, 0 }, // 226
	{ EDGE2 | EDGE3 | EDGE5 | EDGE7 | EDGE8 | EDGE9 | EDGE11, 0, 0, 0 }, // 227
	{ EDGE1 | EDGE2 | EDGE4 | EDGE5 | EDGE8 | EDGE11, 0, 0, 0 }, // 228
	{ EDGE0 | EDGE1 | EDGE2 | EDGE3 | EDGE4 | EDGE5 | EDGE11, 0, 0, 0 }, // 229
	{ EDGE0 | EDGE2 | EDGE4 | EDGE5 | EDGE8 | EDGE9 | EDGE11, 0, 0, 0 }, // 230
	{ EDGE4 | EDGE5 | EDGE9, EDGE2 | EDGE3 | EDGE11, 0, 0 }, // 231
	{ EDGE1 | EDGE2 | EDGE4 | EDGE7 | EDGE9 | EDGE11, 0, 0, 0 }, // 232
	{ EDGE0 | EDGE3 | EDGE8, EDGE1 | EDGE2 | EDGE4 | EDGE7 | EDGE9 | EDGE11, 0, 0 }, // 233
	{ EDGE0 | EDGE2 | EDGE4 | EDGE7 | EDGE11, 0, 0, 0 }, // 234
	{ EDGE2 | EDGE3 | EDGE4 | EDGE7 | EDGE8 | EDGE11, 0, 0, 0 }, // 235
	{ EDGE1 | EDGE2 | EDGE8 | EDGE9 | EDGE11, 0, 0, 0 }, // 236
	{ EDGE0 | EDGE1 | EDGE2 | EDGE3 | EDGE9 | EDGE11, 0, 0, 0 }, // 237
	{ EDGE0 | EDGE2 | EDGE8 | EDGE11, 0, 0, 0 }, // 238
	{ EDGE2 | EDGE3 | EDGE11, 0, 0, 0 }, // 239
	{ EDGE1 | EDGE3 | EDGE5 | EDGE7, 0, 0, 0 }, // 240
	{ EDGE0 | EDGE1 | EDGE5 | EDGE7 | EDGE8, 0, 0, 0 }, // 241
	{ EDGE0 | EDGE3 | EDGE5 | EDGE7 | EDGE9, 0, 0, 0 }, // 242
	{ EDGE5 | EDGE7 | EDGE8 | EDGE9, 0, 0, 0 }, // 243
	{ EDGE1 | EDGE3 | EDGE4 | EDGE5 | EDGE8, 0, 0, 0 }, // 244
	{ EDGE0 | EDGE1 | EDGE4 | EDGE5, 0, 0, 0 }, // 245
	{ EDGE0 | EDGE3 | EDGE4 | EDGE5 | EDGE8 | EDGE9, 0, 0, 0 }, // 246
	{ EDGE4 | EDGE5 | EDGE9, 0, 0, 0 }, // 247
	{ EDGE1 | EDGE3 | EDGE4 | EDGE7 | EDGE9, 0, 0, 0 }, // 248
	{ EDGE0 | EDGE1 | EDGE4 | EDGE7 | EDGE8 | EDGE9, 0, 0, 0 }, // 249
	{ EDGE0 | EDGE3 | EDGE4 | EDGE7, 0, 0, 0 }, // 250
	{ EDGE4 | EDGE7 | EDGE8, 0, 0, 0 }, // 251
	{ EDGE1 | EDGE3 | EDGE8 | EDGE9, 0, 0, 0 }, // 252
	{ EDGE0 | EDGE1 | EDGE9, 0, 0, 0 }, // 253
	{ EDGE0 | EDGE3 | EDGE8, 0, 0, 0 }, // 254
	{ 0, 0, 0, 0 } // 255
};

} // namespace tables

struct VertexData {
	Vector3f position;
	Vector3f normal;

	VertexData(Vector3f p, Vector3f n) : position(p), normal(n) {}
};

inline uint16_t get_point_code(const uint32_t cell_code, const uint16_t edge) {
	uint16_t point_code = 0;
	for (int i = 0; i < 4; ++i) {
		if (tables::dual_points_list[cell_code][i] & edge) {
			point_code = tables::dual_points_list[cell_code][i];
			break;
		}
	}
	return point_code;
}

uint32_t get_vert_index(
		const uint32_t cell_index,
		const uint32_t STEP_X,
		const uint32_t STEP_Y,
		const uint32_t STEP_Z,
		const uint16_t edge,
		const Vector3f vert_offset,
		Span<const float> density_array,
		const float isolevel,
		StdVector<VertexData> &vertex_data,
		robin_hood::unordered_flat_map<uint64_t, uint32_t> &vert_index_map
) {
	// Accesses 2x2x2 voxels where `cell_index` is the one towards negative coordinates, and generates a vertex inside.
	// For normals, a 4x4x4 region may be accessed.

	uint32_t cell_code = 0;
	if (density_array[cell_index] > isolevel) {
		cell_code |= 1;
	}
	if (density_array[cell_index + STEP_X] > isolevel) {
		cell_code |= 2;
	}
	if (density_array[cell_index + STEP_Y] > isolevel) {
		cell_code |= 4;
	}
	if (density_array[cell_index + STEP_X + STEP_Y] > isolevel) {
		cell_code |= 8;
	}
	if (density_array[cell_index + STEP_Z] > isolevel) {
		cell_code |= 16;
	}
	if (density_array[cell_index + STEP_X + STEP_Z] > isolevel) {
		cell_code |= 32;
	}
	if (density_array[cell_index + STEP_Y + STEP_Z] > isolevel) {
		cell_code |= 64;
	}
	if (density_array[cell_index + STEP_X + STEP_Y + STEP_Z] > isolevel) {
		cell_code |= 128;
	}

	const uint16_t point_code = get_point_code(cell_code, edge);

	static constexpr uint32_t point_code_bits = 12;

	const uint64_t lookup = static_cast<uint64_t>(cell_index) << point_code_bits | static_cast<uint64_t>(point_code);
	const uint32_t vert_index = static_cast<uint32_t>(vertex_data.size());
	auto find = vert_index_map.try_emplace(lookup, vert_index);

	if (!find.second) {
		return find.first->second;
	}

	// compute the dual point as the mean of the face vertices belonging to the
	// original marching cubes face
	Vector3f vert;

	// sum edge intersection vertices using the point code
	if (point_code & tables::EDGE0) {
		vert.x += (isolevel - density_array[cell_index]) /
				(density_array[cell_index + STEP_X] - density_array[cell_index]);
	}

	if (point_code & tables::EDGE1) {
		vert.x += 1.0f;
		vert.z += (isolevel - density_array[cell_index + STEP_X]) /
				(density_array[cell_index + STEP_X + STEP_Z] - density_array[cell_index + STEP_X]);
	}

	if (point_code & tables::EDGE2) {
		vert.x += (isolevel - density_array[cell_index + STEP_Z]) /
				(density_array[cell_index + STEP_X + STEP_Z] - density_array[cell_index + STEP_Z]);
		vert.z += 1.0f;
	}

	if (point_code & tables::EDGE3) {
		vert.z += (isolevel - density_array[cell_index]) /
				(density_array[cell_index + STEP_Z] - density_array[cell_index]);
	}

	if (point_code & tables::EDGE4) {
		vert.x += (isolevel - density_array[cell_index + STEP_Y]) /
				(density_array[cell_index + STEP_X + STEP_Y] - density_array[cell_index + STEP_Y]);
		vert.y += 1.0f;
	}

	if (point_code & tables::EDGE5) {
		vert.x += 1.0f;
		vert.y += 1.0f;
		vert.z += (isolevel - density_array[cell_index + STEP_X + STEP_Y]) /
				(density_array[cell_index + STEP_X + STEP_Y + STEP_Z] - density_array[cell_index + STEP_X + STEP_Y]);
	}

	if (point_code & tables::EDGE6) {
		vert.x += (isolevel - density_array[cell_index + STEP_Y + STEP_Z]) /
				(density_array[cell_index + STEP_X + STEP_Y + STEP_Z] - density_array[cell_index + STEP_Y + STEP_Z]);
		vert.y += 1.0f;
		vert.z += 1.0f;
	}

	if (point_code & tables::EDGE7) {
		vert.y += 1.0f;
		vert.z += (isolevel - density_array[cell_index + STEP_Y]) /
				(density_array[cell_index + STEP_Y + STEP_Z] - density_array[cell_index + STEP_Y]);
	}

	if (point_code & tables::EDGE8) {
		vert.y += (isolevel - density_array[cell_index]) /
				(density_array[cell_index + STEP_Y] - density_array[cell_index]);
	}

	if (point_code & tables::EDGE9) {
		vert.x += 1.0f;
		vert.y += (isolevel - density_array[cell_index + STEP_X]) /
				(density_array[cell_index + STEP_X + STEP_Y] - density_array[cell_index + STEP_X]);
	}

	if (point_code & tables::EDGE10) {
		vert.x += 1.0f;
		vert.y += (isolevel - density_array[cell_index + STEP_X + STEP_Z]) /
				(density_array[cell_index + STEP_X + STEP_Y + STEP_Z] - density_array[cell_index + STEP_X + STEP_Z]);
		vert.z += 1.0f;
	}

	if (point_code & tables::EDGE11) {
		vert.y += (isolevel - density_array[cell_index + STEP_Z]) /
				(density_array[cell_index + STEP_Y + STEP_Z] - density_array[cell_index + STEP_Z]);
		vert.z += 1.0f;
	}

	vert /= static_cast<float>(math::count_bits_u16(point_code));

	// Calculate analytical derivative

	const uint32_t deriv_offset_x = STEP_X & static_cast<int>(std::lroundf(-vert.x));
	const uint32_t deriv_offset_y = STEP_Y & static_cast<int>(std::lroundf(-vert.y));
	const uint32_t deriv_offset_z = STEP_Z & static_cast<int>(std::lroundf(-vert.z));

	Vector3f deriv_delta = vert + Vector3f(0.5f);
	deriv_delta -= math::floor(deriv_delta);

	Vector3f derivative;

	uint32_t ncell_index = cell_index;

	for (int32_t z = -1; z < 1; z++) {
		const float contrib_z = std::abs(z + vert.z);

		for (int32_t y = -1; y < 1; y++) {
			const float contrib_y = std::abs(y + vert.y);

			for (int32_t x = -1; x < 1; x++) {
				const float contrib_x = std::abs(x + vert.x);

				if (x) {
					const uint32_t deriv_index = ncell_index + deriv_offset_x;
					derivative.x += contrib_y * contrib_z *
							math::lerp(density_array[deriv_index - STEP_X] - density_array[deriv_index],
									   density_array[deriv_index] - density_array[deriv_index + STEP_X],
									   deriv_delta.x);
				}
				if (y) {
					const uint32_t deriv_index = ncell_index + deriv_offset_y;
					derivative.y += contrib_x * contrib_z *
							math::lerp(density_array[deriv_index - STEP_Y] - density_array[deriv_index],
									   density_array[deriv_index] - density_array[deriv_index + STEP_Y],
									   deriv_delta.y);
				}
				if (z) {
					const uint32_t deriv_index = ncell_index + deriv_offset_z;
					derivative.z += contrib_x * contrib_y *
							math::lerp(density_array[deriv_index - STEP_Z] - density_array[deriv_index],
									   density_array[deriv_index] - density_array[deriv_index + STEP_Z],
									   deriv_delta.z);
				}

				ncell_index += STEP_X;
			}

			ncell_index += STEP_Y - STEP_X * 2;
		}

		ncell_index += STEP_Z - STEP_Y * 2;
	}

	const Vector3f normal = math::normalized(derivative);

	vertex_data.emplace_back(vert + vert_offset, normal);

	return vert_index;
}

static constexpr uint32_t POSITIVE_PADDING = 2;
static constexpr uint32_t NEGATIVE_PADDING = 2;

inline void add_quad_indices(
		const StdVector<VertexData> &vertex_data,
		const uint32_t quad_vert_indices[4],
		const float density,
		const float density_on_axis,
		StdVector<uint32_t> &indices
) {
	// Rotate quad for best vertex lighting
	const uint8_t tri_rotation = 2 *
			(math::dot(vertex_data[quad_vert_indices[0]].normal, vertex_data[quad_vert_indices[3]].normal) <
			 math::dot(vertex_data[quad_vert_indices[1]].normal, vertex_data[quad_vert_indices[2]].normal));

	// Flip tris if backfacing
	const uint8_t tri_flip = 2 * (density < density_on_axis);

	indices.emplace_back(quad_vert_indices[tri_flip]);
	indices.emplace_back(quad_vert_indices[3 - tri_rotation]);
	indices.emplace_back(quad_vert_indices[2 - tri_flip]);
	indices.emplace_back(quad_vert_indices[3 - tri_flip]);
	indices.emplace_back(quad_vert_indices[tri_rotation]);
	indices.emplace_back(quad_vert_indices[1 + tri_flip]);
}

void build_mesh(
		const float isolevel,
		Span<const float> density_values,
		Vector3u32 padded_resolution,
		StdVector<VertexData> &vertex_data,
		StdVector<uint32_t> &indices,
		StdVector<uint32_t> *quad_info
) {
	ZN_PROFILE_SCOPE();

	using namespace zylann;

	constexpr Vector3f VEC_X = Vector3f(1, 0, 0);
	constexpr Vector3f VEC_Y = Vector3f(0, 1, 0);
	constexpr Vector3f VEC_Z = Vector3f(0, 0, 1);

	constexpr uint32_t PADDING = POSITIVE_PADDING + NEGATIVE_PADDING;
	const Vector3u32 unpadded_resolution = padded_resolution - Vector3u32(PADDING);

	const uint32_t STEP_X = padded_resolution.y;
	const uint32_t STEP_Y = 1;
	const uint32_t STEP_Z = padded_resolution.y * padded_resolution.x;

	// TODO Temp allocator?
	robin_hood::unordered_flat_map<uint64_t, uint32_t> vert_index_map;

	Vector3f cell_offset;

	// Start at (2,2,2) within the data grid
	uint32_t cell_index = (STEP_X + STEP_Y + STEP_Z) * 2;

	for (uint32_t z = 0; z < unpadded_resolution.z; z++) {
		cell_offset.z = static_cast<float>(static_cast<int32_t>(z) - 0);

		for (uint32_t x = 0; x < unpadded_resolution.x; x++) {
			cell_offset.x = static_cast<float>(static_cast<int32_t>(x) - 0);

			for (uint32_t y = 0; y < unpadded_resolution.y; y++) {
				cell_offset.y = static_cast<float>(static_cast<int32_t>(y) - 0);

				const float density = density_values[cell_index];

				const float density_x = density_values[cell_index + STEP_X];
				const float density_y = density_values[cell_index + STEP_Y];
				const float density_z = density_values[cell_index + STEP_Z];

				// construct quad for x edge
				{
					// is edge intersected?
					if ((density <= isolevel) ^ (density_x <= isolevel)) {
						if (quad_info != nullptr) {
							quad_info->emplace_back((cell_index << 2) | 0);
						}

						// generate quad
						const uint32_t quad_vert_indices[] = {
							get_vert_index(
									cell_index,
									STEP_X,
									STEP_Y,
									STEP_Z,
									tables::EDGE0,
									cell_offset,
									density_values,
									isolevel,
									vertex_data,
									vert_index_map
							),
							get_vert_index(
									cell_index - STEP_Z,
									STEP_X,
									STEP_Y,
									STEP_Z,
									tables::EDGE2,
									cell_offset - VEC_Z,
									density_values,
									isolevel,
									vertex_data,
									vert_index_map
							),
							get_vert_index(
									cell_index - STEP_Y,
									STEP_X,
									STEP_Y,
									STEP_Z,
									tables::EDGE4,
									cell_offset - VEC_Y,
									density_values,
									isolevel,
									vertex_data,
									vert_index_map
							),
							get_vert_index(
									cell_index - STEP_Y - STEP_Z,
									STEP_X,
									STEP_Y,
									STEP_Z,
									tables::EDGE6,
									cell_offset - (VEC_Y + VEC_Z),
									density_values,
									isolevel,
									vertex_data,
									vert_index_map
							),
						};

						add_quad_indices(vertex_data, quad_vert_indices, density, density_x, indices);
					}
				}

				// construct quad for y edge
				{
					// is edge intersected?
					if ((density <= isolevel) ^ (density_y <= isolevel)) {
						if (quad_info != nullptr) {
							quad_info->emplace_back((cell_index << 2) | 1);
						}
						// generate quad
						const uint32_t quad_vert_indices[] = {
							get_vert_index(
									cell_index,
									STEP_X,
									STEP_Y,
									STEP_Z,
									tables::EDGE8,
									cell_offset,
									density_values,
									isolevel,
									vertex_data,
									vert_index_map
							),
							get_vert_index(
									cell_index - STEP_X,
									STEP_X,
									STEP_Y,
									STEP_Z,
									tables::EDGE9,
									cell_offset - VEC_X,
									density_values,
									isolevel,
									vertex_data,
									vert_index_map
							),
							get_vert_index(
									cell_index - STEP_Z,
									STEP_X,
									STEP_Y,
									STEP_Z,
									tables::EDGE11,
									cell_offset - VEC_Z,
									density_values,
									isolevel,
									vertex_data,
									vert_index_map
							),
							get_vert_index(
									cell_index - STEP_X - STEP_Z,
									STEP_X,
									STEP_Y,
									STEP_Z,
									tables::EDGE10,
									cell_offset - (VEC_X + VEC_Z),
									density_values,
									isolevel,
									vertex_data,
									vert_index_map
							),
						};

						add_quad_indices(vertex_data, quad_vert_indices, density, density_y, indices);
					}
				}

				// construct quad for z edge
				{
					const float density_z = density_values[cell_index + STEP_Z];

					// is edge intersected?
					if ((density <= isolevel) ^ (density_z <= isolevel)) {
						if (quad_info != nullptr) {
							quad_info->emplace_back((cell_index << 2) | 2);
						}

						// generate quad
						const uint32_t quad_vert_indices[] = {
							get_vert_index(
									cell_index,
									STEP_X,
									STEP_Y,
									STEP_Z,
									tables::EDGE3,
									cell_offset,
									density_values,
									isolevel,
									vertex_data,
									vert_index_map
							),
							get_vert_index(
									cell_index - STEP_Y,
									STEP_X,
									STEP_Y,
									STEP_Z,
									tables::EDGE7,
									cell_offset - VEC_Y,
									density_values,
									isolevel,
									vertex_data,
									vert_index_map
							),
							get_vert_index(
									cell_index - STEP_X,
									STEP_X,
									STEP_Y,
									STEP_Z,
									tables::EDGE1,
									cell_offset - VEC_X,
									density_values,
									isolevel,
									vertex_data,
									vert_index_map
							),
							get_vert_index(
									cell_index - STEP_X - STEP_Y,
									STEP_X,
									STEP_Y,
									STEP_Z,
									tables::EDGE5,
									cell_offset - (VEC_X + VEC_Y),
									density_values,
									isolevel,
									vertex_data,
									vert_index_map
							),
						};

						add_quad_indices(vertex_data, quad_vert_indices, density, density_z, indices);
					}
				}

				cell_index += STEP_Y;
			}
			cell_index += STEP_Y * PADDING;
		}
		cell_index += STEP_X * PADDING;
	}
}

struct WeightedIndex {
	uint8_t index;
	uint8_t weight;
};

template <typename TArray>
void insert_combine(TArray &array, uint32_t &num, WeightedIndex e) {
	for (uint32_t i = 0; i < num; ++i) {
		if (array[i].index == e.index) {
			array[i].weight += e.weight;
			return;
		}
	}
	array[num] = e;
	++num;
}

template <typename TArray>
FixedArray<uint8_t, 2> sort_filter_materials(TArray input_materials, uint8_t num) {
	FixedArray<uint8_t, 2> selected_materials;

	// TODO If we have weighted inputs, we can just do evicting insertion sort

	switch (num) {
		case 0:
			ZN_PRINT_ERROR("No materials to sort, this case is unexpected");
			selected_materials[0] = 0;
			selected_materials[1] = 0;
			break;

		case 1:
			selected_materials[0] = input_materials[0].index;
			selected_materials[1] = input_materials[0].index;
			break;

		case 2:
			selected_materials[0] = input_materials[0].index;
			selected_materials[1] = input_materials[1].index;
			break;

		case 3:
			math::sort3_array(input_materials, [](WeightedIndex a, WeightedIndex b) { return a.weight > b.weight; });
			selected_materials[0] = input_materials[0].index;
			selected_materials[1] = input_materials[1].index;
			break;

		case 4:
			math::sort4_array(input_materials, [](WeightedIndex a, WeightedIndex b) { return a.weight > b.weight; });
			selected_materials[0] = input_materials[0].index;
			selected_materials[1] = input_materials[1].index;
			break;

		default: {
			// Rare cases, use general sorting algorithm
			struct Comparator {
				inline bool operator()(const WeightedIndex &a, const WeightedIndex &b) const {
					return a.weight > b.weight;
				}
			};
			SortArray<WeightedIndex, Comparator> sorter;
			sorter.sort(input_materials.data(), num);

			selected_materials[0] = input_materials[0].index;
			selected_materials[1] = input_materials[1].index;
			break;
		}
	}

	return selected_materials;
}

inline Vector3i get_cell_dim_from_axis(uint32_t axis) {
	Vector3i s(3, 3, 3);
	s[axis] -= 1;
	return s;
}

// V1I8_S2I81W8: 1 8-bit material index per voxel, 2 material indices and 1 blend weight per vertex
void build_materials_v_1i8_s_2i8_1w8(
		const StdVector<VertexData> &src_vertices,
		const StdVector<uint32_t> &src_indices,
		const StdVector<uint32_t> &quad_infos,
		const Span<const float> densities,
		const Span<const uint8_t> material_indices,
		const Vector3i padded_resolution,
		StdVector<Vector3f> &dst_vertices,
		StdVector<Vector3f> &dst_normals,
		StdVector<uint32_t> &dst_material_data,
		StdVector<uint32_t> &dst_indices
) {
	ZN_PROFILE_SCOPE();

	// We know DMC generates quads only
	ZN_ASSERT((src_indices.size() % 6) == 0);
	const uint32_t quad_count = src_indices.size() / 6;

	const int32_t STEP_X = padded_resolution.y;
	const int32_t STEP_Y = 1;
	const int32_t STEP_Z = padded_resolution.y * padded_resolution.x;

	// TODO Temp allocator?
	robin_hood::unordered_flat_map<uint64_t, uint32_t> vertex_map;

	const Vector3f vert_offset(2);

	for (uint32_t quad_index = 0; quad_index < quad_count; ++quad_index) {
		const uint32_t src_ii0 = quad_index * 6;

		const uint32_t quad_info = quad_infos[quad_index];
		const uint32_t quad_cell_index = quad_info >> 2;
		const uint32_t quad_axis = quad_info & 3;

		const Vector3i cell_dim = get_cell_dim_from_axis(quad_axis);

		Vector3i coff(1, 1, 1);
		coff[quad_axis] = 0;
		const Vector3i cell_pos_min = Vector3iUtil::from_zxy_index(quad_cell_index, padded_resolution) - coff;

		const Vector3i cell_pos_max = cell_pos_min + cell_dim;

		// TODO We really need info about the cells used for this quad...
		// const uint32_t src_i0 = src_indices[src_ii0];
		// Vector3f min_vert = src_vertices[src_i0].position + vert_offset;
		// Vector3f max_vert = min_vert;
		// for (unsigned int ri = 1; ri < 6; ++ri) {
		// 	const uint32_t src_ii = src_ii0 + ri;
		// 	const uint32_t src_i = src_indices[src_ii];
		// 	const Vector3f v = src_vertices[src_i].position + vert_offset;
		// 	min_vert = math::min(min_vert, v);
		// 	max_vert = math::max(max_vert, v);
		// }

		// // TODO And of course we have to handle the edge case where vertices are ON the side
		// const Vector3i cell_pos_min = to_vec3i(math::floor(min_vert));
		// const Vector3i cell_pos_max = to_vec3i(math::floor(max_vert)) + Vector3iUtil::create(1);
		// // const Vector3i cell_pos_max = to_vec3i(math::ceil(max_vert));

		// const Vector3i cell_dim = cell_pos_max - cell_pos_min;

		// TODO In theory this array could have a lower capacity.
		// Because some voxels will always not be solid, since the quad means there is a surface
		// there.
		FixedArray<WeightedIndex, 3 * 3 * 2> weighted_materials;
		uint32_t num_materials = 0;

		FixedArray<uint8_t, 3 * 3 * 2> material_grid;

		// Find material indices
		// TODO Should we weight them based on vertices already?
		{
			unsigned int ri = 0;
			for (int32_t z = cell_pos_min.z; z < cell_pos_max.z; ++z) {
				for (int32_t x = cell_pos_min.x; x < cell_pos_max.x; ++x) {
					for (int32_t y = cell_pos_min.y; y < cell_pos_max.y; ++y) {
						const uint32_t voxel_index = x * STEP_X + y * STEP_Y + z * STEP_Z;
						const float density = densities[voxel_index];

						// TODO I wish I could automatically exclude empty voxels (SDF>0) from this, but I can't...
						//
						// The motivation for doing this, for example, is for ores: if you have a voxel of gold
						// surrounded by stone, if you remove all matter from the voxel of gold, you'd expect to no
						// longer see gold anywhere. But you will still see it because vertices close to it will
						// interpolate it. Similarly, generating ore patches could end up placing gold in the air and
						// have it become visible, despite there being no solid gold to mine.
						//
						// So we could think, let's automatically replace the material of empty voxels with some default
						// value? That doesn't work here, because due to the dual nature of DMC, surface is generated
						// in between voxels, so we end up "half-tinting" the surface with that default material
						// everywhere, which leads to bad results. It also acts as a "3rd" material in many situations,
						// which makes transitions look blocky because we support only 2 blended materials.
						//
						// But then if we have to sample all materials, those in the air must be consistent with those
						// in the ground...
						//
						// It would be nice if we could somehow really interpolate between only solid voxels, such that
						// the air voxels would not weight into the result at all?

						// const uint8_t material_index = density >= 0.f ? material_indices[voxel_index] : 0;
						const uint8_t material_index = material_indices[voxel_index];
						insert_combine(weighted_materials, num_materials, { material_index, 1 });
						material_grid[ri] = material_index;
						++ri;
					}
				}
			}
		}
		// The first one is the most represented
		FixedArray<uint8_t, 2> selected_materials = sort_filter_materials(weighted_materials, num_materials);
		const uint32_t selected_materials_key =
				static_cast<uint32_t>(selected_materials[0]) | (static_cast<uint32_t>(selected_materials[1]) << 8);
		const uint32_t selected_materials_vd = selected_materials_key << 8;

		// FixedArray<float, 3 * 3 * 2> weight_grid;
		// for (unsigned int i = 0; i < material_grid.size(); ++i) {
		// 	const WeightedIndex src = material_grid[i];
		// 	const float w = src.index == selected_materials[1] ? 1.f : 0.f;
		// 	weight_grid[i] = w;
		// }
		struct WeightGrid {
			uint32_t data = 0;
			inline float operator[](uint32_t i) const {
				return static_cast<float>((data >> i) & 1);
			}
		};
		WeightGrid weight_grid;
		for (unsigned int i = 0; i < material_grid.size(); ++i) {
			const uint8_t src = material_grid[i];
			// If a voxel uses material 1, blend is 1.
			// If a voxel uses material 0, blend is 0.
			// If a voxel uses a material that wasn't selected, fallback on the most represented one, which is also 0.
			// If a voxel is not solid, still use 0.
			const uint32_t m = (src == selected_materials[1] ? 1 : 0);
			weight_grid.data |= (m << i);
		}

		// TODO Avoid computing 6 vertices, there are 4

		const uint32_t rc_step_x = cell_dim.y;
		const uint32_t rc_step_y = 1;
		const uint32_t rc_step_z = cell_dim.y * cell_dim.x;

		for (unsigned int ri = 0; ri < 6; ++ri) {
			const uint32_t src_ii = src_ii0 + ri;
			const uint32_t src_i = src_indices[src_ii];
			const uint32_t dst_i = dst_vertices.size();

			const uint64_t key = static_cast<uint64_t>(src_i << 16) | static_cast<uint64_t>(selected_materials_key);
			const auto res = vertex_map.try_emplace(key, dst_i);
			if (!res.second) {
				// Re-use existing vertex
				dst_indices.emplace_back(res.first->second);
				continue;
			}

			const VertexData vert = src_vertices[src_i];
			const Vector3f vert_position = vert.position + vert_offset;
			const Vector3f fvert = math::floor(vert_position);
			const Vector3i cell_rpos = math::max(to_vec3i(fvert) - cell_pos_min, Vector3iUtil::create(0));
			const uint32_t rci0 = cell_rpos.x * rc_step_x + cell_rpos.y * rc_step_y + cell_rpos.z * rc_step_z;

			const float w0 = weight_grid[rci0];
			const float w1 = weight_grid[rci0 + rc_step_x];
			const float w2 = weight_grid[rci0 + rc_step_y];
			const float w3 = weight_grid[rci0 + rc_step_x + rc_step_y];
			const float w4 = weight_grid[rci0 + rc_step_z];
			const float w5 = weight_grid[rci0 + rc_step_x + rc_step_z];
			const float w6 = weight_grid[rci0 + rc_step_y + rc_step_z];
			const float w7 = weight_grid[rci0 + rc_step_x + rc_step_y + rc_step_z];

			const Vector3f rv = vert_position - fvert;
			const float w = math::interpolate_trilinear_b(w0, w1, w2, w3, w4, w5, w6, w7, rv);

			const uint32_t w8 = math::clamp(static_cast<uint32_t>(w * 255.f), 0u, 255u);
			const uint32_t md = selected_materials_vd | w8;

			dst_vertices.emplace_back(vert.position);
			dst_normals.emplace_back(vert.normal);
			dst_material_data.emplace_back(md);
			dst_indices.emplace_back(dst_i);
		}
	}
}

} // namespace dmc

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VoxelMesherDMC::VoxelMesherDMC() {
	update_padding();
}

VoxelMesherDMC::~VoxelMesherDMC() {}

bool VoxelMesherDMC::is_skirts_enabled() const {
	return _skirts_enabled;
}

void VoxelMesherDMC::set_skirts_enabled(bool enabled) {
	_skirts_enabled = enabled;
	update_padding();
}

VoxelMesherDMC::MaterialMode VoxelMesherDMC::get_material_mode() const {
	return _material_mode;
}

void VoxelMesherDMC::set_material_mode(MaterialMode mode) {
	ZN_ASSERT_RETURN(mode >= 0 && mode < MATERIAL_MODE_COUNT);
	_material_mode = mode;
}

void VoxelMesherDMC::update_padding() {
	if (_skirts_enabled) {
		set_padding(3, 3);
	} else {
		set_padding(2, 2);
	}
}

void VoxelMesherDMC::build(VoxelMesher::Output &output, const VoxelMesher::Input &input) {
	ZN_PROFILE_SCOPE();

	StdVector<float> temp_sd;
	temp_sd.resize(Vector3iUtil::get_volume(input.voxels.get_size()));
	get_unscaled_sdf(input.voxels, to_span(temp_sd));

	for (float &sd : temp_sd) {
		sd *= -1.f;
	}

	const float scale = 1 << input.lod_index;

	// TODO Temp allocator
	StdVector<dmc::VertexData> dmc_vertices;
	// TODO Temp allocator
	StdVector<uint32_t> dmc_indices;
	// TODO Temp allocator
	StdVector<uint32_t> quad_info;

	static constexpr float isolevel = 0.f;

	dmc::build_mesh(
			isolevel,
			to_span(temp_sd),
			to_vec3u32(input.voxels.get_size()),
			dmc_vertices,
			dmc_indices,
			_material_mode != MATERIAL_MODE_NONE ? &quad_info : nullptr
	);

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);

	switch (_material_mode) {
		case MATERIAL_MODE_NONE: {
			if (input.lod_index > 0) {
				for (dmc::VertexData &vd : dmc_vertices) {
					vd.position *= scale;
				}
			}

			PackedVector3Array gd_vertices;
			PackedVector3Array gd_normals;
			gd_vertices.resize(dmc_vertices.size());
			gd_normals.resize(dmc_vertices.size());
			{
				Vector3 *gd_vertices_w = gd_vertices.ptrw();
				Vector3 *gd_normals_w = gd_normals.ptrw();
				for (unsigned int i = 0; i < dmc_vertices.size(); ++i) {
					const dmc::VertexData vd = dmc_vertices[i];
					gd_vertices_w[i] = to_vec3(vd.position);
					gd_normals_w[i] = to_vec3(vd.normal);
				}
			}

			PackedInt32Array gd_indices;
			zylann::godot::copy_to(gd_indices, to_span(dmc_indices).reinterpret_cast_to<int32_t>());

			arrays[Mesh::ARRAY_VERTEX] = gd_vertices;
			arrays[Mesh::ARRAY_NORMAL] = gd_normals;
			arrays[Mesh::ARRAY_INDEX] = gd_indices;
		} break;

		case MATERIAL_MODE_V_1I8_S_2I8_1W8: {
			// TODO Temp allocator
			StdVector<Vector3f> dmc_tex_vertices;
			// TODO Temp allocator
			StdVector<Vector3f> dmc_tex_normals;
			// TODO Temp allocator
			StdVector<uint32_t> dmc_tex_material_data;
			// TODO Temp allocator
			StdVector<uint32_t> dmc_tex_indices;

			const uint32_t material_channel = VoxelBuffer::CHANNEL_INDICES;

			// TODO Temp allocator
			StdVector<uint8_t> backing_material_indices;
			Span<const uint8_t> material_indices;

			if (input.voxels.get_channel_compression(material_channel) == VoxelBuffer::COMPRESSION_UNIFORM) {
				// TODO Optimized version of material building taking predefined indices for the whole chunk
				backing_material_indices.resize(temp_sd.size());
				const uint8_t v = input.voxels.get_voxel(Vector3i(0, 0, 0), material_channel);
				for (uint8_t &dst : backing_material_indices) {
					dst = v;
				}
				material_indices = to_span(backing_material_indices);
			} else {
				ZN_ASSERT_RETURN_MSG(
						input.voxels.get_channel_data_read_only<uint8_t>(material_channel, material_indices),
						"Channel used for material indices has the wrong format"
				);
			}

			dmc::build_materials_v_1i8_s_2i8_1w8(
					dmc_vertices,
					dmc_indices,
					quad_info,
					to_span(temp_sd),
					material_indices,
					input.voxels.get_size(),
					dmc_tex_vertices,
					dmc_tex_normals,
					dmc_tex_material_data,
					dmc_tex_indices
			);

			if (input.lod_index > 0) {
				for (Vector3f &v : dmc_tex_vertices) {
					v *= scale;
				}
			}

			PackedVector3Array gd_vertices;
			PackedVector3Array gd_normals;
			PackedFloat32Array gd_material_data;

			zylann::godot::copy_to(gd_vertices, dmc_tex_vertices);
			zylann::godot::copy_to(gd_normals, dmc_tex_normals);
			zylann::godot::copy_to(gd_material_data, to_span(dmc_tex_material_data).reinterpret_cast_to<const float>());

			PackedInt32Array gd_indices;
			zylann::godot::copy_to(gd_indices, to_span(dmc_tex_indices).reinterpret_cast_to<const int32_t>());

			arrays[Mesh::ARRAY_VERTEX] = gd_vertices;
			arrays[Mesh::ARRAY_NORMAL] = gd_normals;
			arrays[Mesh::ARRAY_CUSTOM0] = gd_material_data;
			arrays[Mesh::ARRAY_INDEX] = gd_indices;

			output.mesh_flags |= (RenderingServer::ARRAY_CUSTOM_R_FLOAT << Mesh::ARRAY_FORMAT_CUSTOM0_SHIFT);
		} break;

		default: {
			ZN_PRINT_ERROR("Unhandled material mode");
		} break;
	}

	output.surfaces.resize(1);
	Output::Surface &surface = output.surfaces.back();
	surface.arrays = arrays;
}

int VoxelMesherDMC::get_used_channels_mask() const {
	return 1 << VoxelBuffer::CHANNEL_SDF;
}

void VoxelMesherDMC::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_skirts_enabled", "channel"), &VoxelMesherDMC::set_skirts_enabled);
	ClassDB::bind_method(D_METHOD("is_skirts_enabled"), &VoxelMesherDMC::is_skirts_enabled);

	ClassDB::bind_method(D_METHOD("set_material_mode", "mode"), &VoxelMesherDMC::set_material_mode);
	ClassDB::bind_method(D_METHOD("get_material_mode"), &VoxelMesherDMC::get_material_mode);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "skirts_enabled"), "set_skirts_enabled", "is_skirts_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "material_mode"), "set_material_mode", "get_material_mode");

	BIND_ENUM_CONSTANT(MATERIAL_MODE_NONE);
	BIND_ENUM_CONSTANT(MATERIAL_MODE_V_1I8_S_2I8_1W8);
}

} // namespace voxel
} // namespace zylann
