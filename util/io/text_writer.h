#ifndef ZN_TEXT_WRITER_H
#define ZN_TEXT_WRITER_H

#include "../containers/span.h"
#include <cstdint>

namespace zylann {

// Interface for an output stream of characters
class TextWriter {
public:
	TextWriter(Span<char> buf);
	virtual ~TextWriter() {}

	void write_char(const char c);
	void write_chars(Span<const char> s);

	void write_c_string(const char *cstr);
	void write_i64(const int64_t i);
	void write_f32(const float f);
	void write_f64(const double f);

	void flush();

	inline Span<const char> get_buffered() const {
		return _buffer.sub(0, _pos);
	}

protected:
	virtual void drain(Span<const char> chars);

	Span<char> _buffer;
	unsigned int _pos;
};

TextWriter &operator<<(TextWriter &w, const bool v);

// TextWriter &operator<<(TextWriter &w, const int8_t v);
// TextWriter &operator<<(TextWriter &w, const int16_t v);
// TextWriter &operator<<(TextWriter &w, const int32_t v);
// TextWriter &operator<<(TextWriter &w, const int64_t v);
// TextWriter &operator<<(TextWriter &w, const uint8_t v);
// TextWriter &operator<<(TextWriter &w, const uint16_t v);
// TextWriter &operator<<(TextWriter &w, const uint32_t v);
// TextWriter &operator<<(TextWriter &w, const uint64_t v);

TextWriter &operator<<(TextWriter &w, const short v);
TextWriter &operator<<(TextWriter &w, const int v);
TextWriter &operator<<(TextWriter &w, const long v);
TextWriter &operator<<(TextWriter &w, const long long v);
TextWriter &operator<<(TextWriter &w, const unsigned short v);
TextWriter &operator<<(TextWriter &w, const unsigned int v);
TextWriter &operator<<(TextWriter &w, const unsigned long v);
TextWriter &operator<<(TextWriter &w, const unsigned long long v);

TextWriter &operator<<(TextWriter &w, const char v);
TextWriter &operator<<(TextWriter &w, const char *v);

TextWriter &operator<<(TextWriter &w, const float v);
TextWriter &operator<<(TextWriter &w, const double v);

} // namespace zylann

#endif // ZN_TEXT_WRITER_H
