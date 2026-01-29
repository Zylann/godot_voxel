#include "text_writer.h"
#include "../string/conv.h"
#include <cstring>

namespace zylann {

TextWriter::TextWriter(Span<char> buf) : _buffer(buf), _pos(0) {}

void TextWriter::drain(Span<const char> chars) {
	ZN_PRINT_ERROR("No implemented sink");
}

void TextWriter::flush() {
	if (_pos > 0) {
		drain(_buffer.sub(0, _pos));
		_pos = 0;
	}
}

void TextWriter::write_char(const char c) {
	if (_pos >= _buffer.size()) {
		flush();
	}
	_buffer[_pos] = c;
	++_pos;
}

void TextWriter::write_chars(Span<const char> s) {
	if (s.size() <= _buffer.size() - _pos) {
		s.copy_to(_buffer.sub(_pos, s.size()));
		_pos += s.size();
	} else {
		// Too big for staging buffer, write directly to sink (which may have higher bounds)
		flush();
		drain(s);
	}
}

void TextWriter::write_c_string(const char *cstr) {
	const size_t len = strlen(cstr);
	write_chars(Span<const char>(cstr, len));
}

void TextWriter::write_i64(const int64_t i) {
	std::array<char, MAX_INT64_CHAR_COUNT_BASE10> buf;
	Span<char> s(buf.data(), buf.size());
	const unsigned int len = int64_to_string_base10(i, s);
	write_chars(s.sub(0, len));
}

void TextWriter::write_f32(const float f) {
	std::array<char, max_float_chars_general<float>()> buf;
	Span<char> s(buf.data(), buf.size());
	const unsigned int len = float32_to_string(f, s);
	write_chars(s.sub(0, len));
}

void TextWriter::write_f64(const double f) {
	std::array<char, max_float_chars_general<double>()> buf;
	Span<char> s(buf.data(), buf.size());
	const unsigned int len = float64_to_string(f, s);
	write_chars(s.sub(0, len));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TextWriter &operator<<(TextWriter &w, const bool v) {
	w.write_c_string(v ? "true" : "false");
	return w;
}

// TextWriter &operator<<(TextWriter &w, const int8_t v) {
// 	w.write_i64(v);
// 	return w;
// }

// TextWriter &operator<<(TextWriter &w, const int16_t v) {
// 	w.write_i64(v);
// 	return w;
// }

// TextWriter &operator<<(TextWriter &w, const int32_t v) {
// 	w.write_i64(v);
// 	return w;
// }

// TextWriter &operator<<(TextWriter &w, const int64_t v) {
// 	w.write_i64(v);
// 	return w;
// }

// TextWriter &operator<<(TextWriter &w, const uint8_t v) {
// 	w.write_i64(v);
// 	return w;
// }

// TextWriter &operator<<(TextWriter &w, const uint16_t v) {
// 	w.write_i64(v);
// 	return w;
// }

// TextWriter &operator<<(TextWriter &w, const uint32_t v) {
// 	w.write_i64(v);
// 	return w;
// }

// TextWriter &operator<<(TextWriter &w, const uint64_t v) {
// 	w.write_i64(v);
// 	return w;
// }

TextWriter &operator<<(TextWriter &w, const short v) {
	w.write_i64(v);
	return w;
}

TextWriter &operator<<(TextWriter &w, const int v) {
	w.write_i64(v);
	return w;
}

TextWriter &operator<<(TextWriter &w, const long v) {
	w.write_i64(v);
	return w;
}

TextWriter &operator<<(TextWriter &w, const long long v) {
	w.write_i64(v);
	return w;
}

TextWriter &operator<<(TextWriter &w, const unsigned short v) {
	w.write_i64(v);
	return w;
}

TextWriter &operator<<(TextWriter &w, const unsigned int v) {
	w.write_i64(v);
	return w;
}

TextWriter &operator<<(TextWriter &w, const unsigned long v) {
	w.write_i64(v);
	return w;
}

TextWriter &operator<<(TextWriter &w, const unsigned long long v) {
	w.write_i64(v);
	return w;
}

TextWriter &operator<<(TextWriter &w, const char v) {
	w.write_char(v);
	return w;
}

TextWriter &operator<<(TextWriter &w, const float v) {
	w.write_f32(v);
	return w;
}

TextWriter &operator<<(TextWriter &w, const double v) {
	w.write_f64(v);
	return w;
}

TextWriter &operator<<(TextWriter &w, const char *v) {
	w.write_c_string(v);
	return w;
}

} // namespace zylann
