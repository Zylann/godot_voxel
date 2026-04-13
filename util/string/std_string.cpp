#include "std_string.h"
#include "../io/text_writer.h"

namespace zylann {

TextWriter &operator<<(TextWriter &w, const StdString &s) {
	w.write_chars(Span<const char>(s.data(), s.size()));
	return w;
}

TextWriter &operator<<(TextWriter &w, const std::string_view s) {
	w.write_chars(Span<const char>(s.data(), s.size()));
	return w;
}

} // namespace zylann
