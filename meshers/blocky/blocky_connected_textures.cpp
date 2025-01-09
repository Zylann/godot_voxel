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

// 0 1 2
// 3 x 4
// 5 6 7
//
// clang-format off
static const uint8_t g_connection_mask_to_case_index[256] = {
	0, 0, 36, 36, 0, 0, 36, 36, 3, 3, 17, 39, 3, 3, 17, 39,
	1, 1, 16, 16, 1, 1, 37, 37, 2, 2, 18, 42, 2, 2, 40, 38,
	0, 0, 36, 36, 0, 0, 36, 36, 3, 3, 17, 39, 3, 3, 17, 39,
	1, 1, 16, 16, 1, 1, 37, 37, 2, 2, 18, 42, 2, 2, 40, 38,
	12, 12, 24, 24, 12, 12, 24, 24, 5, 5, 19, 41, 5, 5, 19, 41,
	4, 4, 6, 6, 4, 4, 30, 30, 7, 7, 46, 20, 7, 7, 8, 11,
	12, 12, 24, 24, 12, 12, 24, 24, 15, 15, 43, 27, 15, 15, 43, 27,
	4, 4, 6, 6, 4, 4, 30, 30, 29, 29, 21, 10, 29, 29, 34, 32,
	0, 0, 36, 36, 0, 0, 36, 36, 3, 3, 17, 39, 3, 3, 17, 39,
	1, 1, 16, 16, 1, 1, 37, 37, 2, 2, 18, 42, 2, 2, 40, 38,
	0, 0, 36, 36, 0, 0, 36, 36, 3, 3, 17, 39, 3, 3, 17, 39,
	1, 1, 16, 16, 1, 1, 37, 37, 2, 2, 18, 42, 2, 2, 40, 38,
	12, 12, 24, 24, 12, 12, 24, 24, 5, 5, 19, 41, 5, 5, 19, 41,
	13, 13, 28, 28, 13, 13, 25, 25, 31, 31, 9, 35, 31, 31, 23, 33,
	12, 12, 24, 24, 12, 12, 24, 24, 15, 15, 43, 27, 15, 15, 43, 27,
	13, 13, 28, 28, 13, 13, 25, 25, 14, 14, 22, 44, 14, 14, 45, 26
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

void generate_atlas_from_compact5(
		const Image &input_image,
		const Rect2i input_rect,
		Image &output_image,
		const Vector2i output_position
) {
	// - - -   - - -   - o -   - o -   o o o
	// - x -   o x o   - x -   o x o   o x o
	// - - -   - - -   - o -   - o -   o o o
	const Vector2i tile_res = Vector2i(input_rect.size.x / 5, input_rect.size.y);
	const Vector2i output_image_size = tile_res * Vector2i(BLOB9_DEFAULT_LAYOUT_SIZE_X, BLOB9_DEFAULT_LAYOUT_SIZE_Y);

	const int xc_len = tile_res.x / 3;
	const int yc_len = tile_res.y / 3;

	const int xm_len = tile_res.x - 2 * xc_len;
	const int ym_len = tile_res.y - 2 * yc_len;

	// clang-format off
	const Rect2i sr00(0,               0,                  xc_len, yc_len);
	const Rect2i sr10(xc_len,          0,                  xm_len, yc_len);
	const Rect2i sr20(xc_len + xm_len, 0,                  xc_len, yc_len);
	const Rect2i sr01(0,               yc_len,             xc_len, ym_len);
	const Rect2i sr11(xc_len,          yc_len,             xm_len, ym_len);
	const Rect2i sr21(xc_len + xm_len, yc_len,             xc_len, ym_len);
	const Rect2i sr02(0,               yc_len + ym_len,    xc_len, yc_len);
	const Rect2i sr12(xc_len,          yc_len + ym_len,    xm_len, yc_len);
	const Rect2i sr22(xc_len + xm_len, yc_len + ym_len,    xc_len, yc_len);
	// clang-format on

	struct TileBuilder {
		const Image &input_image;
		const Vector2i input_layout_origin;
		Image &output_image;
		const Vector2i output_layout_origin;
		const Vector2i tile_res;

		void blit(const Vector2i dst_tile, const Vector2i src_tile, const Rect2i src_tile_relative_rect) {
			const Rect2i src_rect(
					input_layout_origin + src_tile * tile_res + src_tile_relative_rect.position,
					src_tile_relative_rect.size
			);
			const Vector2i dst_pos = output_layout_origin + dst_tile * tile_res + src_tile_relative_rect.position;
			output_image.blit_rect(Ref<Image>(&input_image), src_rect, dst_pos);
		}
	};

	TileBuilder tb{ input_image, input_rect.position, output_image, output_position, tile_res };

	// Case 0
	// - - -
	// - x -
	// - - -
	tb.blit(Vector2i(0, 0), Vector2i(0, 0), Rect2i(Vector2i(), tile_res));

	// Case 1
	// - - -
	// - x o
	// - - -
	tb.blit(Vector2i(1, 0), Vector2i(0, 0), sr00.merge(sr02));
	tb.blit(Vector2i(1, 0), Vector2i(1, 0), sr10.merge(sr22));

	// Case 2
	// - - -
	// o x o
	// - - -
	tb.blit(Vector2i(2, 0), Vector2i(1, 0), Rect2i(Vector2i(), tile_res));

	// Case 3
	// - - -
	// o x -
	// - - -
	tb.blit(Vector2i(3, 0), Vector2i(1, 0), sr00.merge(sr12));
	tb.blit(Vector2i(3, 0), Vector2i(0, 0), sr20.merge(sr22));

	// Case 4
	// - - -
	// - x o
	// - o -
	tb.blit(Vector2i(4, 0), Vector2i(3, 0), sr11.merge(sr22));
	tb.blit(Vector2i(4, 0), Vector2i(2, 0), sr01.merge(sr02));
	tb.blit(Vector2i(4, 0), Vector2i(1, 0), sr10.merge(sr20));
	tb.blit(Vector2i(4, 0), Vector2i(0, 0), sr00);

	// Case 5
	// - - -
	// o x -
	// - o -
	tb.blit(Vector2i(5, 0), Vector2i(3, 0), sr01.merge(sr12));
	tb.blit(Vector2i(5, 0), Vector2i(2, 0), sr21.merge(sr22));
	tb.blit(Vector2i(5, 0), Vector2i(1, 0), sr00.merge(sr10));
	tb.blit(Vector2i(5, 0), Vector2i(0, 0), sr20);

	// Case 6
	// - o -
	// - x o
	// - o -
	tb.blit(Vector2i(6, 0), Vector2i(2, 0), sr00.merge(sr12));
	tb.blit(Vector2i(6, 0), Vector2i(3, 0), sr20.merge(sr22));

	// Case 7
	// - - -
	// o x o
	// - o -
	tb.blit(Vector2i(7, 0), Vector2i(1, 0), sr00.merge(sr20));
	tb.blit(Vector2i(7, 0), Vector2i(3, 0), sr01.merge(sr22));

	// Case 8
	// - o o
	// o x o
	// - o -
	tb.blit(Vector2i(8, 0), Vector2i(3, 0), sr02.merge(sr22));
	tb.blit(Vector2i(8, 0), Vector2i(3, 0), sr00.merge(sr01));
	tb.blit(Vector2i(8, 0), Vector2i(4, 0), sr10.merge(sr21));

	// Case 9
	// - o -
	// o x o
	// - o o
	tb.blit(Vector2i(9, 0), Vector2i(3, 0), sr00.merge(sr20));
	tb.blit(Vector2i(9, 0), Vector2i(3, 0), sr01.merge(sr02));
	tb.blit(Vector2i(9, 0), Vector2i(4, 0), sr11.merge(sr22));

	// Case 10
	// o o -
	// o x o
	// o o -
	tb.blit(Vector2i(10, 0), Vector2i(4, 0), sr00.merge(sr12));
	tb.blit(Vector2i(10, 0), Vector2i(3, 0), sr20.merge(sr22));

	// Case 11
	// o o o
	// o x o
	// - o -
	tb.blit(Vector2i(11, 0), Vector2i(4, 0), sr00.merge(sr21));
	tb.blit(Vector2i(11, 0), Vector2i(3, 0), sr02.merge(sr22));

	// Case 12
	// - - -
	// - x -
	// - o -
	tb.blit(Vector2i(0, 1), Vector2i(0, 0), sr00.merge(sr20));
	tb.blit(Vector2i(0, 1), Vector2i(2, 0), sr01.merge(sr22));

	// Case 13
	// - - -
	// - x o
	// - o o
	tb.blit(Vector2i(1, 1), Vector2i(0, 0), sr00.merge(sr11));
	tb.blit(Vector2i(1, 1), Vector2i(1, 0), sr20.merge(sr21));
	tb.blit(Vector2i(1, 1), Vector2i(2, 0), sr02.merge(sr12));
	tb.blit(Vector2i(1, 1), Vector2i(4, 0), sr22);

	// Case 14
	// - - -
	// o x o
	// o o o
	tb.blit(Vector2i(2, 1), Vector2i(1, 0), sr00.merge(sr20));
	tb.blit(Vector2i(2, 1), Vector2i(4, 0), sr01.merge(sr22));

	// Case 15
	// - - -
	// o x -
	// o o -
	tb.blit(Vector2i(3, 1), Vector2i(0, 0), sr10.merge(sr21));
	tb.blit(Vector2i(3, 1), Vector2i(1, 0), sr00.merge(sr01));
	tb.blit(Vector2i(3, 1), Vector2i(2, 0), sr12.merge(sr22));
	tb.blit(Vector2i(3, 1), Vector2i(4, 0), sr02);

	// Case 16
	// - o -
	// - x o
	// - - -
	tb.blit(Vector2i(4, 1), Vector2i(3, 0), sr10.merge(sr21));
	tb.blit(Vector2i(4, 1), Vector2i(2, 0), sr00.merge(sr01));
	tb.blit(Vector2i(4, 1), Vector2i(1, 0), sr12.merge(sr22));
	tb.blit(Vector2i(4, 1), Vector2i(0, 0), sr02);

	// Case 17
	// - o -
	// o x -
	// - - -
	tb.blit(Vector2i(5, 1), Vector2i(3, 0), sr00.merge(sr11));
	tb.blit(Vector2i(5, 1), Vector2i(2, 0), sr20.merge(sr21));
	tb.blit(Vector2i(5, 1), Vector2i(1, 0), sr02.merge(sr12));
	tb.blit(Vector2i(5, 1), Vector2i(0, 0), sr22);

	// Case 18
	// - o -
	// o x o
	// - - -
	tb.blit(Vector2i(6, 1), Vector2i(3, 0), sr00.merge(sr21));
	tb.blit(Vector2i(6, 1), Vector2i(1, 0), sr02.merge(sr22));

	// Case 19
	// - o -
	// o x -
	// - o -
	tb.blit(Vector2i(7, 1), Vector2i(3, 0), sr00.merge(sr12));
	tb.blit(Vector2i(7, 1), Vector2i(2, 0), sr20.merge(sr22));

	// Case 20
	// o o -
	// o x o
	// - o -
	tb.blit(Vector2i(8, 1), Vector2i(4, 0), sr00.merge(sr11));
	tb.blit(Vector2i(8, 1), Vector2i(3, 0), sr02.merge(sr22));
	tb.blit(Vector2i(8, 1), Vector2i(3, 0), sr20.merge(sr21));

	// Case 21
	// - o -
	// o x o
	// o o -
	tb.blit(Vector2i(9, 1), Vector2i(4, 0), sr01.merge(sr12));
	tb.blit(Vector2i(9, 1), Vector2i(3, 0), sr00.merge(sr20));
	tb.blit(Vector2i(9, 1), Vector2i(3, 0), sr21.merge(sr22));

	// Case 22
	// - o -
	// o x o
	// o o o
	tb.blit(Vector2i(10, 1), Vector2i(4, 0), sr01.merge(sr22));
	tb.blit(Vector2i(10, 1), Vector2i(3, 0), sr00.merge(sr20));

	// Case 23
	// - o o
	// o x o
	// - o o
	tb.blit(Vector2i(11, 1), Vector2i(4, 0), sr10.merge(sr22));
	tb.blit(Vector2i(11, 1), Vector2i(3, 0), sr00.merge(sr02));

	// Case 24
	// - o -
	// - x -
	// - o -
	tb.blit(Vector2i(0, 2), Vector2i(2, 0), sr00.merge(sr22));

	// Case 25
	// - o o
	// - x o
	// - o o
	tb.blit(Vector2i(1, 2), Vector2i(2, 0), sr00.merge(sr02));
	tb.blit(Vector2i(1, 2), Vector2i(4, 0), sr10.merge(sr22));

	// Case 26
	// o o o
	// o x o
	// o o o
	tb.blit(Vector2i(2, 2), Vector2i(4, 0), sr00.merge(sr22));

	// Case 27
	// o o -
	// o x -
	// o o -
	tb.blit(Vector2i(3, 2), Vector2i(4, 0), sr00.merge(sr12));
	tb.blit(Vector2i(3, 2), Vector2i(2, 0), sr20.merge(sr22));

	// Case 28
	// - o -
	// - x o
	// - o o
	tb.blit(Vector2i(4, 2), Vector2i(2, 0), sr00.merge(sr02));
	tb.blit(Vector2i(4, 2), Vector2i(3, 0), sr10.merge(sr20));
	tb.blit(Vector2i(4, 2), Vector2i(4, 0), sr11.merge(sr22));

	// Case 29
	// - - -
	// o x o
	// o o -
	tb.blit(Vector2i(5, 2), Vector2i(1, 0), sr00.merge(sr20));
	tb.blit(Vector2i(5, 2), Vector2i(3, 0), sr21.merge(sr22));
	tb.blit(Vector2i(5, 2), Vector2i(4, 0), sr01.merge(sr12));

	// Case 30
	// - o o
	// - x o
	// - o -
	tb.blit(Vector2i(6, 2), Vector2i(2, 0), sr00.merge(sr02));
	tb.blit(Vector2i(6, 2), Vector2i(3, 0), sr12.merge(sr22));
	tb.blit(Vector2i(6, 2), Vector2i(4, 0), sr10.merge(sr21));

	// Case 31
	// - - -
	// o x o
	// - o o
	tb.blit(Vector2i(7, 2), Vector2i(1, 0), sr00.merge(sr20));
	tb.blit(Vector2i(7, 2), Vector2i(3, 0), sr01.merge(sr02));
	tb.blit(Vector2i(7, 2), Vector2i(4, 0), sr11.merge(sr22));

	// Case 32
	// o o o
	// o x o
	// o o -
	tb.blit(Vector2i(8, 2), Vector2i(4, 0), sr00.merge(sr21));
	tb.blit(Vector2i(8, 2), Vector2i(4, 0), sr02.merge(sr12));
	tb.blit(Vector2i(8, 2), Vector2i(3, 0), sr22);

	// Case 33
	// o o o
	// o x o
	// - o o
	tb.blit(Vector2i(9, 2), Vector2i(4, 0), sr00.merge(sr21));
	tb.blit(Vector2i(9, 2), Vector2i(4, 0), sr12.merge(sr22));
	tb.blit(Vector2i(9, 2), Vector2i(3, 0), sr02);

	// Case 34
	// - o o
	// o x o
	// o o -
	tb.blit(Vector2i(10, 2), Vector2i(3, 0), sr00);
	tb.blit(Vector2i(10, 2), Vector2i(4, 0), sr10.merge(sr20));
	tb.blit(Vector2i(10, 2), Vector2i(4, 0), sr01.merge(sr21));
	tb.blit(Vector2i(10, 2), Vector2i(4, 0), sr02.merge(sr12));
	tb.blit(Vector2i(10, 2), Vector2i(3, 0), sr22);

	// Case 35
	// o o -
	// o x o
	// - o o
	tb.blit(Vector2i(11, 2), Vector2i(3, 0), sr20);
	tb.blit(Vector2i(11, 2), Vector2i(4, 0), sr00.merge(sr10));
	tb.blit(Vector2i(11, 2), Vector2i(4, 0), sr01.merge(sr21));
	tb.blit(Vector2i(11, 2), Vector2i(4, 0), sr12.merge(sr22));
	tb.blit(Vector2i(11, 2), Vector2i(3, 0), sr02);

	// Case 36
	// - o -
	// - x -
	// - - -
	tb.blit(Vector2i(0, 3), Vector2i(2, 0), sr00.merge(sr21));
	tb.blit(Vector2i(0, 3), Vector2i(0, 0), sr02.merge(sr22));

	// Case 37
	// - o o
	// - x o
	// - - -
	tb.blit(Vector2i(1, 3), Vector2i(2, 0), sr00.merge(sr02));
	tb.blit(Vector2i(1, 3), Vector2i(1, 0), sr12.merge(sr22));
	tb.blit(Vector2i(1, 3), Vector2i(0, 0), sr02);
	tb.blit(Vector2i(1, 3), Vector2i(4, 0), sr10.merge(sr21));

	// Case 38
	// o o o
	// o x o
	// - - -
	tb.blit(Vector2i(2, 3), Vector2i(4, 0), sr00.merge(sr21));
	tb.blit(Vector2i(2, 3), Vector2i(1, 0), sr02.merge(sr22));

	// Case 39
	// o o -
	// o x -
	// - - -
	tb.blit(Vector2i(3, 3), Vector2i(4, 0), sr00.merge(sr11));
	tb.blit(Vector2i(3, 3), Vector2i(1, 0), sr02.merge(sr12));
	tb.blit(Vector2i(3, 3), Vector2i(2, 0), sr20.merge(sr21));
	tb.blit(Vector2i(3, 3), Vector2i(0, 0), sr22);

	// Case 40
	// - o o
	// o x o
	// - - -
	tb.blit(Vector2i(4, 3), Vector2i(3, 0), sr00.merge(sr01));
	tb.blit(Vector2i(4, 3), Vector2i(4, 0), sr10.merge(sr21));
	tb.blit(Vector2i(4, 3), Vector2i(1, 0), sr02.merge(sr22));

	// Case 41
	// o o -
	// o x -
	// - o -
	tb.blit(Vector2i(5, 3), Vector2i(4, 0), sr00.merge(sr11));
	tb.blit(Vector2i(5, 3), Vector2i(2, 0), sr20.merge(sr22));
	tb.blit(Vector2i(5, 3), Vector2i(3, 0), sr02.merge(sr12));

	// Case 42
	// o o -
	// o x o
	// - - -
	tb.blit(Vector2i(6, 3), Vector2i(4, 0), sr00.merge(sr11));
	tb.blit(Vector2i(6, 3), Vector2i(3, 0), sr20.merge(sr21));
	tb.blit(Vector2i(6, 3), Vector2i(1, 0), sr02.merge(sr22));

	// Case 43
	// - o -
	// o x -
	// o o -
	tb.blit(Vector2i(7, 3), Vector2i(3, 0), sr00.merge(sr10));
	tb.blit(Vector2i(7, 3), Vector2i(4, 0), sr01.merge(sr12));
	tb.blit(Vector2i(7, 3), Vector2i(2, 0), sr20.merge(sr22));

	// Case 44
	// o o -
	// o x o
	// o o o
	tb.blit(Vector2i(8, 3), Vector2i(4, 0), sr00.merge(sr10));
	tb.blit(Vector2i(8, 3), Vector2i(3, 0), sr20);
	tb.blit(Vector2i(8, 3), Vector2i(4, 0), sr01.merge(sr22));

	// Case 45
	// - o o
	// o x o
	// o o o
	tb.blit(Vector2i(9, 3), Vector2i(3, 0), sr00);
	tb.blit(Vector2i(9, 3), Vector2i(4, 0), sr10.merge(sr20));
	tb.blit(Vector2i(9, 3), Vector2i(4, 0), sr01.merge(sr22));

	// Case 46
	// - o -
	// o x o
	// - o -
	tb.blit(Vector2i(10, 3), Vector2i(3, 0), sr00.merge(sr22));
}

} // namespace zylann::voxel::blocky
