#!/usr/bin/python3
# coding: utf-8

# Utility to embed shader files into C++ files

import os


class Section:
	def __init__(self):
		self.lines = []
		self.tag = ""


def parse_sections(src_lines):
	tag = None
	sections = []
	section = Section()

	for src_line in src_lines:
		sline = src_line.strip()

		prev_section = section

		if sline.startswith('// <') and sline.endswith('>'):
			if sline[4] == '/':
				# Exit tag
				tag = sline[5:len(sline)-1]
				if section.tag == tag:
					sections.append(section)
					section = Section()
			else:
				# Enter tag
				tag = sline[4:len(sline)-1]
				if tag in ['PLACEHOLDER', 'SNIPPET']:
					if len(section.lines) > 0:
						sections.append(section)
					section = Section()
					section.tag = tag

		if prev_section == section:
			section.lines.append(src_line)

	if len(section.lines) > 0:
		sections.append(section)

	return sections


def remove_godot_glsl_header(lines):
	# It appears generated shaders compiled with `shader_compile_spirv_from_source` don't recognize
	# this special directive. Maybe it's not GLSL, maybe it's Godot-specific.
	# There is no documentation at the time, so I have to assume it has to be removed.
	return list(filter(lambda line: not line.strip().startswith('#[compute]'), lines))


TYPE_PLAIN = 0 # Raw text without sections
TYPE_SNIPPET = 1 # Subset of text
TYPE_TEMPLATE = 2 # Text in multiple sections


def classify(sections):
	for section in sections:
		if section.tag == 'PLACEHOLDER':
			return TYPE_TEMPLATE
		elif section.tag == 'SNIPPET':
			return TYPE_SNIPPET
	return TYPE_PLAIN


def cpp_escape(text):
	return text.replace('\\', "\\\\").replace('"', '\\"')


def add_comments_header(lines):
	lines.append("// clang-format off")


def add_comments_footer(lines):
	lines.append("// clang-format on")


def add_cpp_string_literal(src_lines, var_name, lines):
	add_comments_header(lines)

	lines.append("const char *{0} =".format(var_name))

	for src_line in src_lines:
		lines.append('"' + cpp_escape(src_line) + "\\n\"")

	lines[len(lines) - 1] += ';'

	add_comments_footer(lines)


def add_cpp_string_array(src_lines, var_name, lines):
	add_comments_header(lines)
	
	lines.append("const char *{0}[] = {{".format(var_name))

	for src_line in src_lines:
		lines.append('"' + cpp_escape(src_line) + "\\n\",")

	# Null string at the end to act as terminator
	lines.append('0')

	lines.append("};")

	add_comments_footer(lines)


MAX_STRING_LITERAL_SIZE = 65536


def add_cpp_string(src_lines, var_name, lines):
	char_count = 0
	for line in src_lines:
		char_count += len(line)
	if char_count > MAX_STRING_LITERAL_SIZE:
		# print("{0} is over {1} characters ({2}), using string array.".format(
		# 	var_name, MAX_STRING_LITERAL_SIZE, char_count))
		add_cpp_string_array(src_lines, var_name, lines)
	else:
		add_cpp_string_literal(src_lines, var_name, lines)
	lines.append("")


def write_cpp(sections, var_base_name, dst_fpath):
	lines = ["// Generated file", ""]
	index = 0

	for section in sections:
		if len(sections) == 1:
			var_name = var_base_name
		else:
			var_name = "{0}_{1}".format(var_base_name, index)

		add_cpp_string(section.lines, var_name, lines)
		index += 1

	code = '\n'.join(lines)

	with open(dst_fpath, 'w') as f:
		f.write(code)


def get_name_from_path(path):
	return 'g_' + os.path.splitext(os.path.basename(path))[0]


def process_file(src_fpath, dst_fpath):
	lines = []
	with open(src_fpath, 'r') as f:
		lines = f.read().splitlines()

	lines = remove_godot_glsl_header(lines)

	sections = parse_sections(lines)
	ftype = classify(sections)

	if ftype == TYPE_TEMPLATE:
		sections = list(filter(lambda s: s.tag == "", sections))

	elif ftype == TYPE_SNIPPET:
		sections = list(filter(lambda s: s.tag != "", sections))

	else: # plain
		pass

	name = get_name_from_path(dst_fpath)
	write_cpp(sections, name, dst_fpath)


if __name__ == '__main__':
	process_file("dev/block_generator_template.glsl",                 "block_generator_shader_template.h")
	process_file("dev/block_modifier_template.glsl",                  "block_modifier_shader_template.h")
	process_file("dev/detail_gather_hits.glsl",                       "detail_gather_hits_shader.h")
	process_file("dev/detail_generator_template.glsl",                "detail_generator_shader_template.h")
	process_file("dev/detail_normalmap.glsl",                         "detail_normalmap_shader.h")
	process_file("dev/dilate.glsl",                                   "dilate_normalmap_shader.h")
	process_file("dev/modifier_mesh_snippet.glsl",                    "modifier_mesh_shader_snippet.h")
	process_file("dev/modifier_sphere_snippet.glsl",                  "modifier_sphere_shader_snippet.h")
	process_file("dev/transvoxel_minimal.gdshader",                   "transvoxel_minimal_shader.h")
	process_file("dev/fast_noise_lite/fast_noise_lite.gdshaderinc",   "fast_noise_lite_shader.h")

