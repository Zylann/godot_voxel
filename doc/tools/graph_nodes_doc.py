#!/usr/bin/python3
# coding: utf-8 

import xml.etree.ElementTree as ET
import sys
import textwrap
import bbcode
import bbcode_to_markdown
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
            desc = bbcode_to_markdown.format_text_for_table(node.description, module_class_names, None, 'api/')
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
            out += bbcode_to_markdown.format_text(desc, module_class_names, None, 'api/')
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
        f.write(bbcode_to_markdown.format_text(
            "This page lists all nodes that can be used in [VoxelGeneratorGraph] and [VoxelGraphFunction].\n\n", 
            module_class_names, None, 'api/'))
        f.write(md)

