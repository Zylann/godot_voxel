#ifndef ZYLANN_COLOR8_H
#define ZYLANN_COLOR8_H

#include "color.h"

namespace zylann {

// Color with 8-bit components. Lighter to store than its floating-point counterpart.
struct Color8 {
	union {
		struct {
			uint8_t r;
			uint8_t g;
			uint8_t b;
			uint8_t a;
		};
		uint8_t components[4];
		uint32_t packed_value;
	};

	Color8() : r(0), g(0), b(0), a(0) {}

	Color8(uint8_t p_r, uint8_t p_g, uint8_t p_b, uint8_t p_a) : r(p_r), g(p_g), b(p_b), a(p_a) {}

	Color8(Color c) {
		r = c.r * 255;
		g = c.g * 255;
		b = c.b * 255;
		a = c.a * 255;
	}

	static inline Color8 from_u8(uint8_t v) {
		// rrggbbaa
		// Each color component is in 0..3, bring back to 0..255
		// 0, 85, 170, 255
		return Color8( //
				(v >> 6) * 85, //
				((v >> 4) & 3) * 85, //
				((v >> 2) & 3) * 85, //
				(v & 3) * 85);
	}

	static inline Color8 from_u16(uint16_t v) {
		// rrrrgggg bbbbaaaa 🐐
		// Each color component is in 0..15, bring back to 0..255
		//   0,  17,  34,  51,
		//  68, 85,  102, 119,
		// 136, 153, 170, 187,
		// 204, 221, 238, 255
		return Color8( //
				(v >> 12) * 17, //
				((v >> 8) & 0xf) * 17, //
				((v >> 4) & 0xf) * 17, //
				(v & 0xf) * 17);
	}

	static inline Color8 from_u32(uint32_t c) {
		// rrrrrrrr gggggggg bbbbbbbb aaaaaaaa
		return Color8(c >> 24, (c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff);
	}

	inline uint8_t to_u8() const {
		// Lossy
		return ((r >> 6) << 6) | //
				((g >> 6) << 4) | //
				((b >> 6) << 2) | //
				(a >> 6);
	}

	inline uint16_t to_u16() const {
		// Lossy
		return ((r >> 4) << 12) | //
				((g >> 4) << 8) | //
				((b >> 4) << 4) | //
				(a >> 4);
	}

	inline uint32_t to_u32() const {
		return (r << 24) | (g << 16) | (b << 8) | a;
	}

	operator Color() const {
		return Color(r / 255.f, g / 255.f, b / 255.f, a / 255.f);
	}

	inline bool operator==(const Color8 &p_v) const {
		return packed_value == p_v.packed_value;
	}

	inline bool operator!=(const Color8 &p_v) const {
		return packed_value != p_v.packed_value;
	}
};

} // namespace zylann

#endif // ZYLANN_COLOR8_H
