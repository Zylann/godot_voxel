
const STRUCT_SINGLE_LITTERAL = 0
const STRUCT_NULL_TERMINATED_ARRAY = 0


# func _ready():
# 	export_to_cpp("fnl.gdshader", "fnl.cpp", "<fnl>", "</fnl>", 
#		STRUCT_NULL_TERMINATED_ARRAY)


static func export_to_cpp(src_fpath: String, dst_fpath: String, var_name : String, 
	structure_mode := STRUCT_SINGLE_LITTERAL):
	
	var f = FileAccess.open(src_fpath, FileAccess.READ)
	var err = FileAccess.get_open_error()
	if err != OK:
		push_error(str("Could not open file ", src_fpath, ", error ", err))
		return
	var src_lines = read_lines(f)
	# Well that's how you close a file now
	f = null
	
	# It appears generated shaders compiled with `shader_compile_spirv_from_source` don't recognize
	# this special directive. Maybe it's not GLSL, maybe it's Godot-specific.
	# There is no documentation at the time, so I have to assume it has to be removed.
	for i in len(src_lines):
		var line = src_lines[i]
		if line.strip_edges().begins_with("#[compute]"):
			src_lines.remove_at(i)
			break
	
	var sections = find_sections(src_lines)

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
	
	f = FileAccess.open(dst_fpath, FileAccess.WRITE)
	err = FileAccess.get_open_error()
	if err != OK:
		push_error(str("Could not open file ", src_fpath, ", error ", err))
		return
	f.store_string(cpp_text)


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


static func find_sections(lines: Array) -> Array:
	var current_section = []
	var sections = []
	var in_placeholder_section = false
	sections.append(current_section)
	for line in lines:
		if in_placeholder_section:
			if line.contains("// </PLACEHOLDER_SECTION>"):
				in_placeholder_section = false
		else:
			if line.contains("// <PLACEHOLDER_SECTION>"):
				current_section = []
				sections.append(current_section)
				in_placeholder_section = true
			else:
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
