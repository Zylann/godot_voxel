
const STRUCT_SINGLE_LITTERAL = 0
const STRUCT_NULL_TERMINATED_ARRAY = 1


# func _ready():
# 	export_to_cpp("fnl.gdshader", "fnl.cpp", "<fnl>", "</fnl>", 
#		STRUCT_NULL_TERMINATED_ARRAY)


static func parse_file_sections_from_file(fpath: String, tags: Array, strip_header: bool) -> Array:
	var f = FileAccess.open(fpath, FileAccess.READ)
	var err = FileAccess.get_open_error()
	if err != OK:
		push_error(str("Could not open file ", fpath, ", error ", err))
		return []
	var src_lines = read_lines(f)
	# Well that's how you close a file now
	f = null
	
	if strip_header:
		# It appears generated shaders compiled with `shader_compile_spirv_from_source` don't recognize
		# this special directive. Maybe it's not GLSL, maybe it's Godot-specific.
		# There is no documentation at the time, so I have to assume it has to be removed.
		for i in len(src_lines):
			var line = src_lines[i]
			if line.strip_edges().begins_with("#[compute]"):
				src_lines.remove_at(i)
				break
	
	var sections = find_sections(src_lines, tags)
	return sections


static func generate_file(template_path: String, snippet_path: String, dst_path: String):
	var template_sections = parse_file_sections_from_file(template_path, [
		"// <PLACEHOLDER_SECTION>",	
		"// </PLACEHOLDER_SECTION>"
	], false)
	var snippet_sections = parse_file_sections_from_file(snippet_path, [
		"// <SNIPPET>",	
		"// </SNIPPET>"
	], true)
	var sections = [
		template_sections[0],
		snippet_sections[1],
		template_sections[2]
	]
	
	var text = ""
	for section in sections:
		text += "\n".join(PackedStringArray(section))
	
	var f = FileAccess.open(dst_path, FileAccess.WRITE)
	var err = FileAccess.get_open_error()
	if err != OK:
		push_error(str("Could not open file ", dst_path, ", error ", err))
		return
	f.store_string(text)


static func export_snippet_to_cpp(snippet_path: String, dst_path: String, var_name: String,
	structure_mode := STRUCT_SINGLE_LITTERAL):
	
	var snippet_sections = parse_file_sections_from_file(snippet_path, [
		"// <SNIPPET>",	
		"// </SNIPPET>"
	], true)
	
	if len(snippet_sections) == 0:
		return

	var section : Array = snippet_sections[1]
	
	var cpp_code = generate_cpp_code([section], var_name, structure_mode)
	write_text_to_file(cpp_code, dst_path)


static func generate_cpp_code(sections: Array, var_name : String, structure_mode: int) -> String:
	var lines = []
	lines.append("// Generated file")
	lines.append("")
	
	for section_index in len(sections):
		lines.append("// clang-format off")
		var section = sections[section_index]
		
		var section_name
		if len(sections) == 1:
			section_name = var_name
		else:
			section_name = str(var_name, "_", section_index)
		
		if structure_mode == STRUCT_SINGLE_LITTERAL:
			generate_code_single_literal(section, lines, section_name)
		elif structure_mode == STRUCT_NULL_TERMINATED_ARRAY:
			append_null_terminated_array_of_lines(section, lines, section_name)
		else:
			assert(false)
		
		lines.append("// clang-format on")
		lines.append("")
	
	var cpp_text = "\n".join(PackedStringArray(lines))
	return cpp_text


static func export_to_cpp(src_fpath: String, dst_fpath: String, var_name : String, 
	structure_mode := STRUCT_SINGLE_LITTERAL):
	
	var sections = parse_file_sections_from_file(src_fpath, [
		"// <PLACEHOLDER_SECTION>",	
		"// </PLACEHOLDER_SECTION>"
	], true)
	
	if len(sections) == 0:
		return

	if len(sections) == 3:
		sections.remove_at(1)

	var cpp_text = generate_cpp_code(sections, var_name, structure_mode)
	write_text_to_file(cpp_text, dst_fpath)


static func write_text_to_file(text: String, fpath: String):
	var f = FileAccess.open(fpath, FileAccess.WRITE)
	var err = FileAccess.get_open_error()
	if err != OK:
		push_error(str("Could not open file ", fpath, ", error ", err))
		return
	f.store_string(text)


static func read_lines_subset(f: FileAccess, begin_marker: String, end_marker: String) -> Array:
	assert(end_marker != "")
	var lines = []
	var record = false
	while not f.eof_reached():
		var line = f.get_line()
		if line.find(begin_marker) != -1:
			record = true
			continue
		elif line.find(end_marker) != -1:
			break
		if record:
			lines.append(line)
	return lines


static func read_lines(f: FileAccess) -> Array:
	var lines = []
	while not f.eof_reached():
		var line = f.get_line()
		lines.append(line)
	return lines


static func find_sections(lines: Array, tags: Array) -> Array:
	if len(tags) == 0:
		return [lines.duplicate()]

	var current_section = []
	var sections = []
	sections.append(current_section)

	var tag_index = 0

	for line in lines:
		if tag_index < len(tags) and line.contains(tags[tag_index]):
			tag_index += 1
			current_section = []
			sections.append(current_section)
			continue
		
		current_section.append(line)

	return sections


static func append_null_terminated_array_of_lines(
	src_lines: Array, dst_lines: Array, var_name: String):
	
	dst_lines.append(str("const char *", var_name, "[] = {"))
	for i in len(src_lines):
		var line = src_lines[i]
		dst_lines.append(str("\"", escape(line), "\\n\","))
	dst_lines.append("0};")


static func generate_code_single_literal(src_lines: Array, dst_lines: Array, var_name: String):
	dst_lines.append(str("const char *", var_name, " = "))
	for i in len(src_lines):
		var line = src_lines[i]
		var text = str("\"", escape(line), "\\n\"")
		if i + 1 == len(src_lines):
			text += ";"
		dst_lines.append(text)


# Kinda crappy
#static func generate_code_minified(lines: Array) -> String:
#	var text = "const char *GDSHADER_SOURCE = "
#	var mtext = ""
#	for i in len(lines):
#		var line = lines[i]
#		mtext += escape(line).replace("  ", " ").replace("\n", "")
#	var count = 0
#	text += "\n\""
#	for i in len(mtext):
#		text += mtext[i]
#		count += 1
#		if count == 95:
#			text += "\"\n"
#			count = 0
#			if i + 1 < len(mtext):
#				text += "\""
#	text += "\";\n"
#	return text


static func escape(s: String) -> String:
	return s.replace("\\", "\\\\").replace("\"", "\\\"")
