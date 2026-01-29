#ifndef ZN_STD_STRING_TEXT_WRITER_H
#define ZN_STD_STRING_TEXT_WRITER_H

#include "../string/std_string.h"
#include "text_writer.h"

namespace zylann {

// TextWriter writing to a string with no staging buffer
class StdStringTextWriter : public TextWriter {
public:
	StdStringTextWriter() : TextWriter(Span<char>()) {}

	inline const StdString &get_written() const {
		return _str;
	}

protected:
	void drain(Span<const char> chars) override {
		_str += std::string_view(chars.data(), chars.size());
	}

	StdString _str;
};

} // namespace zylann

#endif // ZN_STD_STRING_TEXT_WRITER_H
