#ifndef ZYLANN_NON_COPYABLE_H
#define ZYLANN_NON_COPYABLE_H

namespace zylann {

class NonCopyable {
protected:
	NonCopyable() = default;
	~NonCopyable() = default;

	NonCopyable(NonCopyable const &) = delete;
	void operator=(NonCopyable const &x) = delete;
};

} // namespace zylann

#endif // ZYLANN_NON_COPYABLE_H
