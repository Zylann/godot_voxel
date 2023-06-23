#ifndef ZN_GODOT_XML_PARSER_H
#define ZN_GODOT_XML_PARSER_H

#if defined(ZN_GODOT)
#include <core/io/xml_parser.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/xml_parser.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_XML_PARSER_H
