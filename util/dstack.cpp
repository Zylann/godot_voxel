#include "dstack.h"
#include "fixed_array.h"
#include "string_funcs.h"

#include <string>

namespace zylann {
namespace dstack {

struct Stack {
	FixedArray<Frame, 64> frames;
	unsigned int count = 0;
};

Stack &get_tls_stack() {
	thread_local Stack tls_stack;
	return tls_stack;
}

void push(const char *file, unsigned int line, const char *function) {
	Stack &stack = get_tls_stack();
	stack.frames[stack.count] = { file, function, line };
	++stack.count;
}

void pop() {
	Stack &stack = get_tls_stack();
	--stack.count;
}

Info::Info() {
	const Stack &stack = get_tls_stack();
	_frames.resize(stack.count);
	for (unsigned int i = 0; i < stack.count; ++i) {
		_frames[i] = stack.frames[i];
	}
}

void Info::to_string(FwdMutableStdString s) const {
	for (unsigned int i = 0; i < _frames.size(); ++i) {
		const Frame &frame = _frames[i];
		s.s += format("{} ({}:{})\n", frame.function, frame.file, frame.line);
	}
}

} // namespace dstack
} // namespace zylann
