#include "../../generators/graph/node_type_db.h"
#include "../../util/errors.h"
#include "../../util/godot/classes/file_access.h"
#include "../../util/godot/classes/xml_parser.h"
#include "../../util/godot/core/array.h"
#include <unordered_map>
#include <vector>

namespace zylann::voxel {

// Temporary data used instead of directly modifying NodeTypeDB
struct GraphNodeDocumentation {
	String name;
	String description;
	String category;
};

GraphNodeDocumentation *find_node_by_name(std::vector<GraphNodeDocumentation> &nodes, String name) {
	for (GraphNodeDocumentation &node : nodes) {
		if (node.name == name) {
			return &node;
		}
	}
	return nullptr;
}

bool parse_graph_nodes_doc_xml(XMLParser &parser, std::vector<GraphNodeDocumentation> &nodes) {
	ZN_ASSERT_RETURN_V(parser.read() == OK, false);

	// For some reason Godot considers `<?xml` as `NODE_UNKNOWN` and `get_node_name` returns the ENTIRE line without
	// `<`, unlike what I assumed when reading Godot's doctool.
	// doctool gets away with it because it has a `NODE_UNKNOWN` fallback so nobody noticed.
	// See https://github.com/godotengine/godot/issues/72517
	//
	// ZN_ASSERT_RETURN_V(parser.get_node_type() == XMLParser::NODE_ELEMENT, false);
	// ZN_ASSERT_RETURN_V(parser.get_node_name() == "?xml", false);
	// parser.skip_section();
	ZN_ASSERT_RETURN_V(parser.read() == OK, false);

	ZN_ASSERT_RETURN_V(parser.get_node_type() == XMLParser::NODE_ELEMENT, false);
	ZN_ASSERT_RETURN_V(parser.get_node_name() == "nodes", false);

	while (parser.read() == OK) {
		if (parser.get_node_type() == XMLParser::NODE_ELEMENT) {
			if (parser.get_node_name() == "node") {
				ZN_ASSERT_RETURN_V(parser.has_attribute("name"), false);
				const String node_name = parser.get_named_attribute_value("name");

				GraphNodeDocumentation *node = find_node_by_name(nodes, node_name);
				if (node == nullptr) {
					ZN_PRINT_WARNING("Unknown node name in XML");
					parser.skip_section();

				} else {
					if (parser.has_attribute("category")) {
						node->category = parser.get_named_attribute_value("category");
					}
				}

				while (parser.read() == OK) {
					if (parser.get_node_type() == XMLParser::NODE_ELEMENT) {
						// We don't parse everything, only what is provided from the XML file as source of truth.
						// Other things' source of truth are coming from C++ and will be written later.

						if (parser.get_node_name() == "input") {
							continue;
						}
						if (parser.get_node_name() == "output") {
							continue;
						}
						if (parser.get_node_name() == "parameter") {
							continue;
						}

						if (parser.get_node_name() == "description") {
							ZN_ASSERT_RETURN_V(parser.read() == OK, false);

							if (parser.get_node_type() == XMLParser::NODE_TEXT) {
								node->description = parser.get_node_data();
							}

							ZN_ASSERT_RETURN_V(parser.read() == OK, false);
							// End of <description>
							ZN_ASSERT_RETURN_V(parser.get_node_type() == XMLParser::NODE_ELEMENT_END, false);

							continue;
						}

						ZN_PRINT_WARNING("Unknown XML node");

					} else if (parser.get_node_type() == XMLParser::NODE_ELEMENT_END) {
						// End of <node>
						break;
					}
				}

			} else {
				ZN_PRINT_WARNING("Unknown XML node");
			}

		} else if (parser.get_node_type() == XMLParser::NODE_ELEMENT_END) {
			// End of <nodes>
			break;
		}
	}

	return true;
}

String strip_eols(String text) {
	String out;
	int pos = 0;
	for (; pos < text.length(); ++pos) {
		char32_t c = text[pos];
		if (c == '\r' || c == '\n') {
			continue;
		}
		break;
	}
	const int begin = pos;
	for (pos = text.length() - 1; pos >= 0; --pos) {
		char32_t c = text[pos];
		if (c == '\r' || c == '\n') {
			continue;
		}
		break;
	}
	const int end = pos + 1;
	return text.substr(begin, end - begin);
}

void write_graph_nodes_doc_xml(
		FileAccess &f, const std::vector<GraphNodeDocumentation> &nodes_doc, const pg::NodeTypeDB &type_db) {
	class CodeWriter {
	public:
		CodeWriter(FileAccess &f) : _f(f) {}

		void write_line(String s, bool newline = true) {
			for (int i = 0; i < _indent_level; ++i) {
				_f.store_string("\t");
			}
			if (newline) {
				s += "\n";
			}
			_f.store_string(s);
		}

		void indent() {
			++_indent_level;
		}

		void dedent() {
			ZN_ASSERT(_indent_level > 0);
			--_indent_level;
		}

		void write_text(String text) {
			String rt = reformat_text(text, _indent_level);
			if (rt.is_empty()) {
				return;
			}
			_f.store_string(rt.xml_escape() + "\n");
		}

	private:
		static int get_indent_level(String s) {
			int space_count = 0;
			int tab_count = 0;
			for (int i = 0; i < s.length(); ++i) {
				const char32_t c = s[i];
				if (c == ' ') {
					++space_count;
				} else if (c == '\t') {
					++tab_count;
				} else {
					break;
				}
			}
			return tab_count + space_count / 4;
		}

		// Text nodes are parsed as the entire series of characters between `>` and `<`, which is inconvenient when
		// writing back indented docs
		static String reformat_text(String text, int p_indent_level) {
			PackedStringArray lines = text.split("\n");

			while (lines.size() > 0 && lines[0].strip_edges().is_empty()) {
				lines.remove_at(0);
			}

			while (lines.size() > 0 && lines[lines.size() - 1].strip_edges().is_empty()) {
				lines.remove_at(lines.size() - 1);
			}

			{
				// Doing this so the code is the same with GDExtension...
				String *lines_p = lines.ptrw();
				for (int line_index = 0; line_index < lines.size(); ++line_index) {
					String s = lines[line_index];

					String indent;
					const int indent_level = get_indent_level(s);
					for (int i = 0; i < indent_level; ++i) {
						indent += "\t";
					}

					lines_p[line_index] = indent + s.strip_edges();
				}
			}

			return String("\n").join(lines);
		}

	private:
		FileAccess &_f;
		int _indent_level = 0;
	};

	CodeWriter w{ f };

	w.write_line("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>");

	w.write_line("<nodes>");
	w.indent();

	for (const GraphNodeDocumentation &node_doc : nodes_doc) {
		w.write_line(String("<node name=\"{0}\" category=\"{1}\">").format(varray(node_doc.name, node_doc.category)));
		w.indent();

		pg::VoxelGraphFunction::NodeTypeID type_id;
		ZN_ASSERT_RETURN(type_db.try_get_type_id_from_name(node_doc.name, type_id));
		const pg::NodeType &type = type_db.get_type(type_id);

		for (const pg::NodeType::Port &input : type.inputs) {
			w.write_line(String("<input name=\"{0}\" default_value=\"{1}\"/>")
								 .format(varray(input.name, input.default_value)));
		}

		for (const pg::NodeType::Port &output : type.outputs) {
			ZN_ASSERT_CONTINUE(!output.name.is_empty());
			if (output.name[0] == '_') {
				// Internal
				continue;
			}
			w.write_line(String("<output name=\"{0}\"/>").format(varray(output.name)));
		}

		for (const pg::NodeType::Param &param : type.params) {
			w.write_line(String("<parameter name=\"{0}\" type=\"{1}\" default_value=\"{2}\"/>")
								 .format(varray(param.name,
										 param.type == Variant::OBJECT ? param.class_name
																	   : Variant::get_type_name(param.type),
										 param.default_value == Variant() ? "null" : param.default_value)));
		}

		w.write_line("<description>");
		w.indent();

		w.write_text(node_doc.description);

		w.dedent();
		w.write_line("</description>");

		w.dedent();
		w.write_line("</node>");
	}

	w.dedent();
	w.write_line("</nodes>");
}

// Parses an XML file containing documentation about graph nodes,
// Adds information from known nodes in the engine, and merges it with the information that was in the XML file,
// Removes information from nodes that are no longer in the engine,
// and writes the result to an XML file.
void run_graph_nodes_doc_tool(String src_xml_fpath, String dst_xml_fpath) {
	ZN_PRINT_VERBOSE("Running Voxel graph nodes doc tool");

	Ref<XMLParser> parser = memnew(XMLParser);
	{
		const Error err = parser->open(src_xml_fpath);
		if (err != OK) {
			return;
		}
	}

	const pg::NodeTypeDB &type_db = pg::NodeTypeDB::get_singleton();

	std::vector<GraphNodeDocumentation> nodes;

	// First populate with all known node types
	for (int node_type_id = 0; node_type_id < type_db.get_type_count(); ++node_type_id) {
		const pg::NodeType &type = type_db.get_type(node_type_id);

		GraphNodeDocumentation doc;
		doc.name = type.name;

		nodes.push_back(doc);
	}

	// Parse descriptions and categories, which come from the XML file
	ZN_ASSERT_RETURN(parse_graph_nodes_doc_xml(**parser, nodes));

	struct GraphNodeDocumentationComparer {
		bool operator()(const GraphNodeDocumentation &a, const GraphNodeDocumentation &b) const {
			return a.name < b.name;
		}
	};
	SortArray<GraphNodeDocumentation, GraphNodeDocumentationComparer> sorter;
	sorter.sort(nodes.data(), nodes.size());

	Ref<FileAccess> fa = FileAccess::open(dst_xml_fpath, FileAccess::WRITE);
	ZN_ASSERT_RETURN_MSG(fa.is_valid(), "Failed to write XML file");

	// Write XML file back with merged information
	write_graph_nodes_doc_xml(**fa, nodes, type_db);

	ZN_PRINT_VERBOSE("Voxel graph nodes doc tool is done.");
}

} // namespace zylann::voxel
