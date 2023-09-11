#!/usr/bin/python3
# coding: utf-8 

import xml.etree.ElementTree as ET
import sys
import textwrap
import bbcode
import markdown
from pathlib import Path


class GNode:
    def __init__(self):
        self.name = ""
        self.category = ""
        self.description = ""
        self.inputs = []
        self.outputs = []
        self.parameters = []


class GInput:
    def __init__(self):
        self.name = ""
        self.default_value = "0"


class GOutput:
    def __init__(self):
        self.name = ""


class GParameter:
    def __init__(self):
        self.name = ""
        self.type = ""
        self.default_value = ""


def parse_nodes_xml(src_fpath):
    # Parse XML
    tree = ET.parse(src_fpath)
    root = tree.getroot()
    if root.tag != "nodes":
        # TODO Should we use exceptions instead?
        print("Error: Not a valid nodes XML file!\n")
        sys.exit(1)

    nodes = []

    nodes_xml = root.findall('node')

    for node_xml in nodes_xml:
        node = GNode()
        
        node.name = node_xml.attrib['name']

        if 'category' in node_xml.attrib:
            node.category = node_xml.attrib['category']

        inputs_xml = node_xml.findall('input')
        for input_xml in inputs_xml:
            input = GInput()
            input.name = input_xml.attrib['name']
            input.default_value = input_xml.attrib['default_value']
            node.inputs.append(input)

        outputs_xml = node_xml.findall('output')
        for output_xml in outputs_xml:
            output = GOutput()
            output.name = output_xml.attrib['name']
            node.outputs.append(output)

        params_xml = node_xml.findall('parameter')
        for param_xml in params_xml:
            param = GParameter()
            param.name = param_xml.attrib['name']
            param.type = param_xml.attrib['type']
            param.default_value = param_xml.attrib['default_value']
            node.parameters.append(param)

        desc_xml = node_xml.find('description')
        node.description = desc_xml.text

        nodes.append(node)

    return nodes


def format_doc_bbcodes_for_markdown(text, multiline, module_class_names, current_class_name):
    bb_nodes = bbcode.parse(text)

    in_codeblock = False
    url = None

    out = ""
    for bb_node in bb_nodes:
        if isinstance(bb_node, bbcode.NodeText):
            node_text = bb_node.text

            if not multiline:
                # Replace newlines.
                if in_codeblock:
                    # So far that's for displaying a list of things in descriptions that are shown in a table,
                    # so let's remove newlines and use commas.
                    node_text = ' ' + ', '.join(node_text.strip().splitlines())
                else:
                    node_text = ' '.join(node_text.splitlines())

            elif not in_codeblock:
                # Godot's BBCode docs don't have an explicit way to define paragraphs.
                # It seems newlines mean paragraphs, and there are no "manual line breaks".
                # So we insert a second newline after non-empty lines.
                lines = node_text.splitlines(True)
                lines2 = []
                for line in lines:
                    lines2.append(line)
                    if line.strip() != "" and '\n' in line:
                        lines2.append('\n')

                # lines2 = [lines[0]]
                # for line_index in range(1, len(lines)):
                #     line = lines[line_index]
                #     if line.strip() != "" and lines[line_index - 1].strip() != "":
                #         lines2.append("\n")
                #     lines2.append(line)

                # if "library." in node_text:
                #     print("--- CV")
                #     print(lines)
                #     print("--- To")
                #     print(lines2)
                #     print("---")
                node_text = ''.join(lines2)
            
            if url != None:
                out += markdown.make_link(node_text, url)
                url = None

            else:
                out += node_text

        elif isinstance(bb_node, bbcode.NodeTag):
            if bb_node.name == 'codeblock':
                if multiline:
                    # Can't tell which language it is. Godot actually introduced a [codeblocks] tag which can contain 
                    # a block for every script language Godot supports.
                    out += '```'
                else:
                    out += '`'
                in_codeblock = bb_node.is_opening()

            # Specific to voxel module
            elif bb_node.name == 'graph_node':
                out += "`{0}`".format(bb_node.get_first_option_key())
            
            elif bb_node.name == 'code':
                out += '`'

            elif bb_node.name == 'url':
                if bb_node.is_opening():
                    url = bb_node.value
            
            elif bb_node.name == 'member' \
            or bb_node.name == 'method' \
            or bb_node.name == 'enum' \
            or bb_node.name == "constant":
                member = bb_node.get_first_option_key()

                # Get class name and member name
                member_dot_index = member.find('.')
                if member_dot_index != -1:
                    class_name = member[0 : member_dot_index]
                    member_name = member[member_dot_index + 1:]
                else:
                    if current_class_name is None:
                        raise Exception(bb_node.name + \
                            " BBCode was used without class name, but there is no current class in this context.")
                    class_name = current_class_name
                    member_name = member

                # Generate Markdown
                if bb_node.name == 'member':
                    out += markdown.make_property_link(class_name, member_name, 'api/', module_class_names)
                elif bb_node.name == 'method':
                    out += markdown.make_method_link(class_name, member_name, 'api/', module_class_names)
                elif bb_node.name == 'enum':
                    out += markdown.make_enum_link(class_name, member_name, 'api/', module_class_names)
                elif bb_node.name == 'constant':
                    out += markdown.make_constant_link(class_name, member_name, 'api/', module_class_names)
                else:
                    raise Exception("Unhandled case")

            else:
                # Class lookup: assuming name convention, 
                # otherwise we need a complete list of classes and it's a bit cumbersome to obtain
                if bb_node.name[0].isupper():
                    out += markdown.make_type(bb_node.name, 'api/', module_class_names)

                else:
                    # Error fallback
                    print("Unhandled BBCode in Markdown translation:", bb_node.to_string())
                    print(text)
                    out += bb_node.to_string()

    return out


def format_text_for_markdown_table(text, module_class_names, current_class_name):
    lines = text.splitlines()

    for i in range(0, len(lines)):
        lines[i] = lines[i].strip()

    # Newlines aren't supported, but what to replace them with depends on BBCode
    text = '\n'.join(lines)

    text = format_doc_bbcodes_for_markdown(text, False, module_class_names, current_class_name)

    return text


def format_text_for_markdown(text, module_class_names, current_class_name):
    text = textwrap.dedent(text)
    md = format_doc_bbcodes_for_markdown(text, True, module_class_names, current_class_name)
    # Stripping because due to some newline-related workarounds, we may have introduced extra trailing lines,
    # and there may also be unwanted leading lines. Normally Markdown renderers ignore those, but it's cleaner.
    # Note: we don't use Markdown's indentation syntax (which would break if it begins the text).
    md = md.strip()
    # print("--- Converting text ---")
    # print(text)
    # print(">>> to")
    # print(md)
    # print("---")
    return md


def get_nodes_by_category_dict(nodes_list):
    default_category_name = "Other"
    nodes_per_category = {}

    for node in nodes_list:
        category_name = node.category

        if node.category == "":
            category_name = default_category_name

        if category_name not in nodes_per_category:
            nodes_per_category[category_name] = []

        nodes_per_category[category_name].append(node)
    
    return nodes_per_category


# Formats nodes in a table. Was used before, but Mkdocs is bad at it,
# long descriptions are not wrapped on multiple lines and instead have scrollbars...
def write_markdown_table_from_nodes(nodes, module_class_names):
    out = ""
    nodes_per_category = get_nodes_by_category_dict(nodes)
    category_names = sorted(nodes_per_category.keys())

    for category_name in category_names:
        out += "## " + category_name + "\n\n"

        table_rows = [["Node name", "Description"]]

        for node in nodes_per_category[category_name]:
            desc = format_text_for_markdown_table(node.description, module_class_names, None)
            table_rows.append([node.name, desc])

        out += markdown.make_table(table_rows)
        out += "\n\n"

    return out


def strip_leading_and_trailing_empty_lines(text, newline = '\n'):
    lines = text.splitlines()

    # Remove leading empty lines
    while len(lines) > 0 and lines[0].strip() == "":
        del lines[0]

    # Remove trailing empty lines
    while len(lines) > 0 and lines[-1].strip() == "":
        del lines[-1]

    text = newline.join(lines)
    return text


# More classic representation, can show more things
def write_markdown_listing_from_nodes(nodes, module_class_names):
    out = ""
    nodes_per_category = get_nodes_by_category_dict(nodes)
    category_names = sorted(nodes_per_category.keys())

    for category_name in category_names:
        out += "## " + category_name + "\n\n"

        for node in nodes_per_category[category_name]:
            out += "### " + node.name + "\n\n"

            if len(node.inputs) > 0:
                out += "Inputs: " + ", ".join(['`' + port.name + '`' for port in node.inputs]) + "\n"
            if len(node.outputs) > 0:
                out += "Outputs: " + ", ".join(['`' + port.name + '`' for port in node.outputs]) + "\n"
            if len(node.parameters) > 0:
                out += "Parameters: " + ", ".join(['`' + param.name + '`' for param in node.parameters]) + "\n"
            
            out += "\n"
            desc = strip_leading_and_trailing_empty_lines(node.description)
            out += format_text_for_markdown(desc, module_class_names, None)
            out += "\n\n"
    
    return out


def format_doc_bbcodes_to_cpp(text):
    bb_nodes = bbcode.parse(text)

    out = ""
    for bb_node in bb_nodes:
        if isinstance(bb_node, bbcode.NodeText):
            out += bb_node.text

        elif isinstance(bb_node, bbcode.NodeTag):
            if bb_node.name == 'codeblock':
                if bb_node.is_opening():
                    out += "[code]"
                else:
                    out += "[/code]"

            elif bb_node.name == 'graph_node':
                out += '[code]' + bb_node.get_first_option_key() + '[/code]'
            
            else:
                # Class lookup: assuming name convention, 
                # otherwise we need a complete list of classes and it's a bit cumbersome to obtain
                if bb_node.name[0].isupper():
                    out += "[url={0}]{0}[/url]".format(bb_node.name)

                else:
                    # Fallback to RichTextLabel's built-in tags
                    out += bb_node.to_string()

    return out


def format_text_for_cpp(text):
    # Remove common whitespace from the beginning of each line
    text = textwrap.dedent(text)

    text = strip_leading_and_trailing_empty_lines(text, "\\n")
    text = format_doc_bbcodes_to_cpp(text)
    
    text = text.replace('"', '\\"')
    text = text.replace('\\[', '[')

    return text


def write_cpp_from_nodes(nodes):
    out = ""

    out += "// <GENERATED>\n"
    out += "// clang-format off\n"
    out += "namespace GraphNodesDocData {\n\n"
    
    out += "struct Node {\n"
    out += "    const char *name;\n"
    out += "    const char *category;\n"
    out += "    const char *description;\n"
    out += "};\n\n"
    
    out += "static const unsigned int COUNT = " + str(len(nodes)) + ";\n"
    out += "static const Node g_data[COUNT] = {\n"

    for node in nodes:
        out += "    {{\"{0}\", \"{1}\", \"{2}\"}},\n".format(node.name, node.category, format_text_for_cpp(node.description))

    out += "};\n\n"

    out += "} // namespace GraphNodesDocData\n"
    out += "// clang-format on\n"
    out += "// </GENERATED>\n"

    return out


def get_module_class_names(xml_classes_dir):
    xml_files = list(xml_classes_dir.glob("*.xml"))

    class_names = []
    for xml_file in xml_files:
        class_names.append(xml_file.stem)
    
    return class_names


if __name__ == "__main__":
    # Updates Markdown and C++ documentation from XML data

    xml_nodes_fpath = "../graph_nodes.xml"
    xml_classes_dirpath = "../classes"
    cpp_fpath = "../../editor/graph/graph_nodes_doc_data.h"
    md_fpath = "../source/graph_nodes.md"

    nodes = parse_nodes_xml(xml_nodes_fpath)

    # Generate C++
    cpp = write_cpp_from_nodes(nodes)
    with open(cpp_fpath, "w") as f:
        f.write(cpp)
    
    # Generate Markdown
    module_class_names = get_module_class_names(Path(xml_classes_dirpath))
    md = write_markdown_listing_from_nodes(nodes, module_class_names)
    with open(md_fpath, "w") as f:
        f.write("# VoxelGeneratorGraph nodes\n\n")
        f.write(format_text_for_markdown(
            "This page lists all nodes that can be used in [VoxelGeneratorGraph] and [VoxelGraphFunction].\n\n", 
            module_class_names, None))
        f.write(md)

