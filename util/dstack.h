#ifndef ZN_DSTACK_H
#define ZN_DSTACK_H

#include "fwd_std_string.h"
#include <vector>

#ifdef DEBUG_ENABLED
#define ZN_DSTACK_ENABLED
#endif

#ifdef ZN_DSTACK_ENABLED
// Put this macro on top of each function you want to track in debug stack traces.
#define ZN_DSTACK() zylann::dstack::Scope dstack_scope_##__LINE__(__FILE__, __LINE__, __FUNCTION__)
#else
#define ZN_DSTACK()
#endif

namespace zylann {
namespace dstack {

void push(const char *file, unsigned int line, const char *fname);
void pop();

struct Scope {
	Scope(const char *file, unsigned int line, const char *function) {
		push(file, line, function);
	}
	~Scope() {
		pop();
	}
};

struct Frame {
	const char *file = nullptr;
	const char *function = nullptr;
	unsigned int line = 0;
};

struct Info {
public:
	// Constructs a copy of the current stack gathered so far from ZN_DSTACK() calls
	Info();
	void to_string(FwdMutableStdString s) const;

private:
	std::vector<Frame> _frames;
};

} // namespace dstack
} // namespace zylann

#endif // ZN_DSTACK_H
