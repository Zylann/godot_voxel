#ifndef ZN_FWD_STD_STRING_H
#define ZN_FWD_STD_STRING_H

namespace zylann {

// std::string can't be forward-declared. Using type-tunneling instead.
// http://jonjagger.blogspot.com/2011/04/forward-declaring-stdstring-in-c.html
struct FwdConstStdString;
struct FwdMutableStdString;

} // namespace zylann

#endif // ZN_FWD_STD_STRING_H
