#include "container_funcs.h"
#include "log.h"
#include "string_funcs.h"

namespace zylann {

void print_data_hex(Span<const uint8_t> data) {
	std::stringstream ss;
	struct L {
		static inline char to_hex(uint8_t nibble) {
			if (nibble < 10) {
				return '0' + nibble;
			} else {
				return 'a' + (nibble - 10);
			}
		}
	};
	ss << "---";
	ss << std::endl;
	ss << "Data size: ";
	ss << data.size();
	for (unsigned int i = 0; i < data.size(); ++i) {
		if ((i % 16) == 0) {
			ss << std::endl;
			ss << i;
			const unsigned int margin = 6;
			unsigned int p = 10;
			for (unsigned int t = 0; t < margin; ++t) {
				if (i < p) {
					ss << ' ';
				}
				p *= 10;
			}
			ss << " | ";
		}
		const uint8_t b = data[i];
		const uint8_t low_nibble = b & 0xf;
		const uint8_t high_nibble = (b >> 4) & 0xf;
		ss << L::to_hex(high_nibble);
		ss << L::to_hex(low_nibble);
		ss << " ";
	}
	ss << std::endl;
	ss << "---";
	println(ss.str());
}

} // namespace zylann
