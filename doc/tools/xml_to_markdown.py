#!/usr/bin/python3
# coding: utf-8 

# Converts a Godot Class XML file to an MD file
#
# There is nothing configurable in this file
# Run it without arguments for usage


import sys
import re
import xml.etree.ElementTree as ET
from time import gmtime, strftime
import os


godot_classes_url = "https://docs.godotengine.org/en/stable/classes"


def make_link(text, url):
    return "[" + text + "](" + url + ")"


def make_type(name, module_class_names):
    if name == "void":
        link = "#"
    elif name in module_class_names:
        link = name + ".md"
    else:
        link = godot_classes_url + "/class_" + name.lower() + ".html"
    return make_link(name, link)


# Assumes text is dedented
def format_regular_text(text, module_class_names):
    # Doubling line endings otherwise Mkdocs doesn't give us enough space
    text = text.replace('\n', '\n\n').replace('[code]', '`').replace('[/code]', '`')
    s = ""
    while True:
        i = text.find('[')
        if i == -1:
            s += text
            break
        s += text[:i]
        text = text[i + 1:]
        i = text.find(']')
        # There must be a closing bracket
        assert i != -1
        cmd = text[:i]
        text = text[i + 1:]

        if cmd.startswith('url='):
            # [url=xxx]text[/url]
            url = cmd[len('url='):]
            i = text.find('[/url]')
            assert i != -1
            link_text = text[:i]
            s += make_link(link_text, url)
            text = text[i + len('[/url]'):]

        elif cmd.find(' ') == -1:
            # [typename]
            s += make_type(cmd, module_class_names)
        else:
            # TODO Enhancement: members and shit
            s += cmd
    return s


# Removes all indentation found in common with all lines
def dedent(text):
    lines = text.splitlines()
    min_count = 99
    for line in lines:
        if line.strip() == "":
            continue
        count = 0
        for c in line:
            if c == '\t':
                count += 1
            else:
                break
        min_count = min(count, min_count)
    dedented_lines = []
    for line in lines:
        dedented_lines.append(line[min_count:])
    return "\n".join(dedented_lines)


def make_text(text, module_class_names):
    text = dedent(text)
    s = ""
    while True:
        i = text.find("[codeblock]")
        if i == -1:
            s += format_regular_text(text, module_class_names)
            break
        s += format_regular_text(text[:i], module_class_names)
        text = text[i + len("[codeblock]"):]
        s += "```gdscript"
        i = text.find("[/codeblock]")
        # There must be a closing tag
        assert i != -1
        s += re.sub(r'^\s\s', '', text[:i])
        text = text[i + len("[/codeblock]"):]
        s += "\n```"
    return s.strip()


def make_single_line_text(text):
    s = text.strip()
    s = re.sub(r'\s\s+', r' ', s)
    s = s.replace("[code]", '`')
    s = s.replace("[/code]", '`')
    return s


# `table` is a list of rows. Rows are list of strings.
def make_table(table):
    if len(table) == 0:
        return ""
    column_count = len(table[0])
    column_widths = [0] * column_count
    for row in table:
        assert len(row) == column_count
        for i, text in enumerate(row):
            column_widths[i] = max(len(text), column_widths[i])
    column_widths = [w + 1 for w in column_widths]
    #total_width = sum(column_widths) + len(column_widths) + 1
    s = "\n"
    for row_index, row in enumerate(table):
        for column_index, text in enumerate(row):
            if column_index > 0:
                s += " | "
            s += text
            colw = column_widths[column_index]
            for i in range(len(text), colw):
                s += " "
        if row_index == 0:
            s += "\n"
            for column_index in range(0, column_count):
                if column_index > 0:
                    s += " | "
                colw = column_widths[column_index]
                for i in range(0, colw):
                    s += "-"
        s += "\n"
    # Hack to force some space after tables. Mkdocs sticks next contents too close otherwise.
    s += "<p></p>"
    return s


# `args` is a list of XML elements
def make_arglist(args, module_class_names):
    s = "("
    for arg_index, arg in enumerate(args):
        if arg_index > 0:
            s += ","
        s += " " + make_type(arg.attrib['type'], module_class_names) + " " + arg.attrib['name']
        if 'default' in arg.attrib:
            s += "=" + arg.attrib['default']
    s += " )"
    return s


# `items` is a list of XML elements
def make_constants(items):
    s = ""
    for item in items:
        s += "- **" + item.attrib['name'] + "** = **" + item.attrib['value'] + "**"
        text = item.text.strip()
        if text != "":
            s += " --- " + make_single_line_text(item.text)
        s += "\n"
    return s


def make_custom_internal_link(name):
    assert name.find(' ') == -1
    return make_link(name, "#i_" + name)


# This is a hack we can do because Markdown allows to fallback on HTML
def make_custom_internal_anchor(name):
    return '<span id="i_' + name + '"></span>'


# `f_xml` is the path to the source XML file.
# `f_out` is the path to the destination file. Use '-' for to print to stdout.
# `module_class_names` is a list of strings. Each string is a class name.
def process_xml(f_xml, f_out, module_class_names):
    #print("Parsing", f_xml)

    # Parse XML
    tree = ET.parse(f_xml)
    root = tree.getroot()
    if root.tag != "class":
        print("Error: No class found. Not a valid Godot class XML file!\n")
        sys.exit(1)

    # Header
    out = "# " + root.attrib['name'] + "\n\n"
    out += "Inherits: " + make_type(root.attrib['inherits'], module_class_names) + "\n\n"
    out += "\n"
    out += make_text(root.find('brief_description').text, module_class_names) + "\n\n"

    text = make_text(root.find('description').text, module_class_names)
    if text != "":
        out += "## Description: \n\n" + text + "\n\n"

    # Tutorials
    tutorials = root.find('tutorials')
    if tutorials is not None:
        links = tutorials.findall('link')
        if len(links) > 0:
            out += "## Tutorials: \n\n"
            for link in links:
                out += "- [" + link.attrib['title'] + "](" + link.text + ")\n"

    # Properties summary
    members = []
    members_container = root.find('members')
    if members_container is not None:
        members = members_container.findall('member')
        if len(members) > 0:
            out += "## Properties: \n\n"
            table = [["Type", "Name", "Default"]]
            for member in members:
                row = [
                    "`" + member.attrib['type'] + "`",
                    make_custom_internal_link(member.attrib['name'])
                ]
                if 'default' in member.attrib:
                    row.append(member.attrib['default'])
                else:
                    row.append("")
                table.append(row)
            out += make_table(table)
            out += "\n\n"
        
    # Methods summary
    methods = []
    methods_container = root.find('methods')
    if methods_container is not None:
        methods = methods_container.findall('method')

        if len(methods) > 0:
            out += "## Methods: \n\n"
            table = [["Return", "Signature"]]

            # TODO Remove from list if it's a getter/setter of a property
            for method in methods:
                signature = make_custom_internal_link(method.attrib['name']) + " "
                args = method.findall('param')
                signature += make_arglist(args, module_class_names)
                signature += " "

                if 'qualifiers' in method.attrib:
                    signature += method.attrib['qualifiers']

                return_node = method.find('return')

                row = [
                    make_type(return_node.attrib['type'], module_class_names),
                    signature
                ]
                table.append(row)

            out += make_table(table)
            out += "\n\n"
    
    # Signals
    signals = []
    signals_container = root.find('signals')
    if signals_container is not None:
        signals = signals_container.findall('signal')

        if len(signals) > 0:
            out += "## Signals: \n\n"

            for signal in signals:
                out += "- "
                out += signal.attrib['name']

                args = signal.findall('param')
                out += make_arglist(args, module_class_names)
                out += " \n\n"

                desc = signal.find('description')
                if desc is not None:
                    text = make_text(desc.text, module_class_names)
                    if text != "":
                        out += text
                        out += "\n\n"
    
    # Enumerations and constants
    constants_container = root.find('constants')
    if constants_container is not None:
        generic_constants = constants_container.findall('constant')

        enums = {}
        constants = []

        for generic_constant in generic_constants:
            if 'enum' in generic_constant.attrib:
                name = generic_constant.attrib['enum']
                if name in enums:
                    enum_items = enums[name]
                else:
                    enum_items = []
                    enums[name] = enum_items
                enum_items.append(generic_constant)
            else:
                constants.append(generic_constant)
        
        # Enums
        if len(enums) > 0:
            out += "## Enumerations: \n\n"

            for enum_name, enum_items in enums.items():
                out += "enum **" + enum_name + "**: \n\n"
                out += make_constants(enum_items)
                out += "\n"
            
            out += "\n"

        # Constants
        if len(constants) > 0:
            out += "## Constants: \n\n"
            out += make_constants(constants)
            out += "\n"
    
    # Property descriptions
    if len(members) > 0:
        out += "## Property Descriptions\n\n"

        for member in members:
            out += "- " + make_type(member.attrib['type'], module_class_names) \
                + make_custom_internal_anchor(member.attrib['name']) + " **" + member.attrib['name'] + "**"
            if 'default' in member.attrib:
                out += " = " + member.attrib['default']
            out += "\n\n"

            if member.text is not None:
                text = make_text(member.text, module_class_names)
                if text != "":
                    out += text
                    out += "\n"

            out += "\n"
    
    # Method descriptions
    if len(methods) > 0:
        out += "## Method Descriptions\n\n"

        for method in methods:
            return_node = method.find('return')
            out += "- " + make_type(return_node.attrib['type'], module_class_names) \
                + make_custom_internal_anchor(method.attrib['name']) + " **" + method.attrib['name'] + "**"
            args = method.findall('param')
            out += make_arglist(args, module_class_names)
            out += " "
            if 'qualifiers' in method.attrib:
                signature += method.attrib['qualifiers']

            out += "\n\n"

            desc = method.find('description')
            if desc is not None:
                text = make_text(desc.text, module_class_names)
                if text != "":
                    out += text
                    out += "\n"
            
            out += "\n"

    # Footer
    out += "_Generated on " + strftime("%b %d, %Y", gmtime()) + "_\n" 
    #Full time stamp "%Y-%m-%d %H:%M:%S %z"

    if f_out == '-':
        print(out)
    else:
        outfile = open(f_out, mode='a', encoding='utf-8')
        outfile.write(out)


def process_xml_folder(src_dir, dst_dir, verbose):
    # Make output dir and remove old files
    if not os.path.isdir(dst_dir):
        if verbose:
            print("Making output directory: " + dst_dir)
        os.makedirs(dst_dir)

    for i in dst_dir.glob("*.md"):
        if verbose:
            print("Removing old: ", i)
        os.remove(i)

    # Convert files to MD
    xml_files = list(src_dir.glob("*.xml"))
    doc_files = []
    count = 0

    class_names = []
    for xml_file in xml_files:
        class_names.append(xml_file.stem)
    
    for src in xml_files:
        dest = dst_dir / (src.stem + ".md")
        if verbose:
            print("Converting ", src, dest)
        process_xml(src, dest, class_names)
        count += 1
        doc_files.append(dest)

    #generate_class_index(output_path, doc_files, verbose)
    #count += 1

    print("Generated %d files in %s." % (count, dst_dir))


###########################
## Main()

# If called from command line
if __name__ == "__main__":
    # Print usage if no args
    if len(sys.argv) < 2:
        print("Usage: %s infile [outfile]" % sys.argv[0])
        print("Prints to stdout if outfile is not specified.\n")
        sys.exit(0)

    # Input file
    infile = sys.argv[1]

    # Print to screen if no output
    if len(sys.argv) < 3:
        outfile = "-"
    else: 
        outfile = sys.argv[2]

    process_xml(infile, outfile, [])

