#!/usr/bin/python
# coding: utf-8 

# Converts a Godot Class XML file to an MD file
#
# There is nothing configurable in this file
# Run it without arguments for usage


import sys
import re
import xml.etree.ElementTree as ET
from time import gmtime, strftime


########################################
## Functions

# Get an attribute of the specified node 
# Returns "" on failure; safe for concatenation
#
# Apply prefix/suffix: "before %s after" 
#
# Example use:
# 
# Get i.attrib['name'] if it exists:
#
#   get_attr(i, 'name')             
# 
# Get a child element of i called <return>, get its .attrib['type'],
# prepend " -> ", and append "\n", but only if <return> and 'type' exist.
#
#   get_attr(i.find('return'), 'type', " -> %s\n")  
#
def get_attr(node, key, string="%s"):
    if node is None:
        return ""

    if key in node.attrib:
        subs = string.split("%s", 1)
        return subs[0] + node.attrib[key] + subs[1]
    else:
        return ""


# Gets the text from a block, or from a named child block (e.g <description>)
# Prefix/suffix string format: "before %s after" 
# Returns "" on failure, safe for concatenation
def get_text(node, key, string="%s", reformat=True):
    if node is None:
        return ""

    if key == "":               # If no key, use the specified node
        n = node
    else:
        n = node.find(key)      # If key, search for first child named key

    if n is not None:
        if reformat:
            out = n.text.strip()
            out = re.sub(r'\s\s+', r'\n\n', out)
        else:
            out = n.text
        if out != "":
            subs = string.split("%s", 1)

            return subs[0] + out + subs[1]

    return ""


# Processes Godot's property/function/signals/constants xml
def dump_group (node, search, header):
    output = "## " + header + "\n\n"
    elems = node.findall(search)
    for i in elems:

        # Element delimeter
        output += "#### » "  

        # Method return type
        output += get_attr(i.find('return'), 'type', "%s ")

        # Property Type 
        output += get_attr(i, 'type', "%s ")

        # Enum Constant 
        output += get_attr(i, 'enum', "%s.")

        # Name of element
        output += get_attr(i, 'name')


        # Method arguments, types, defaults, and qualifiers
        if i.tag == 'method':
            output += " ( "

            args = i.findall('argument')
            for j in args:
                output += get_attr(j, 'type', "%s ") + get_attr(j, 'name')

                output += get_attr(j, 'default', "=%s")

                if j != args[-1]:
                    output += ", "

            output += " ) " + get_attr(i, 'qualifiers', " %s") 
            output += "\n"


        # Enum value
        output += get_attr(i, 'value', " = %s\n")


        # Properties: Setter/Getters 
        if i.tag=='member': 
            output += "\n"
        output += get_attr(i, 'setter', "\n`%s (value)` setter\n")
        output += get_attr(i, 'getter', "\n`%s ()` getter\n")

        # Any text within the current block
        output += get_text(i, '', "\n%s\n\n")

        # Any <description> child
        output += get_text(i, 'description', "\n%s\n\n")


        output += "\n\n"


    output += "\n"
    return output


# Use '-' for f_out to print to stdout
def process_xml(f_xml, f_out):

    # Parse XML and XSLT
    tree = ET.parse(f_xml)
    root = tree.getroot()
    if root.tag != "class":
        print ("Error: No class found. Not a valid Godot class XML file!\n")
        sys.exit(1)


    # Header
    out  = get_attr(root, 'name', "# Class: %s\n\n")
    out += get_attr(root, 'inherits', "Inherits: %s\n\n")
    out += get_attr(root, 'version', "_Godot version: %s_\n\n")
    out += "\n"

    out += get_text(tree, './/brief_description', "## Brief Description: \n\n%s\n\n\n")

    out += get_text(tree, './/description', "## Class Description: \n\n%s\n\n\n")

    e = tree.find(".//tutorials")
    if e is not None:
        out += "## Online Tutorials: \n\n"
        out += get_text(e, '', "%s\n")          # Any general text in tutorials
        for i in e: 
            out += get_text(i, '', "* %s\n")
        out += "\n\n"


    # Main body sections
    out += dump_group(tree, "constants/constant", "Constants:")

    out += dump_group(tree, "members/member", "Properties:")

    out += dump_group(tree, "methods/method", "Methods:")

    out += dump_group(tree, "signals/signal", "Signals:")


    # Format BB code
    out = out.replace("[code]", "`")
    out = out.replace("[/code]", "`")
    out = out.replace("[b]", "**")
    out = out.replace("[/b]", "**")

    out = re.sub(r'\[method (\S+)\]', r'`\1()`', out)
    out = re.sub(r'\[(member )?(\S+)\]', r'`\2`', out)


    # Footer

    out += "---\n* [Class List](Class_List.md)\n* [Doc Index](../01_get-started.md)\n\n"
    out += "_Generated on " + strftime("%b %d, %Y", gmtime()) + "_\n" 
    #Full time stamp "%Y-%m-%d %H:%M:%S %z"

    if f_out == '-':
        print(out)
    else:
        outfile = open(f_out, 'a')
        outfile.write(out)



###########################
## Main()

def main():
    pass


# If called from command line
if __name__ == "__main__":

    # Print usage if no args
    if len(sys.argv) < 2:
        print("Usage: %s infile [outfile]" % sys.argv[0]);
        print("Prints to stdout if outfile is not specified.\n");
        sys.exit(0)

    # Input file
    infile = sys.argv[1]

    # Print to screen if no output
    if len(sys.argv) < 3:
        outfile = "-"
    else: 
        outfile = sys.argv[2]

    process_xml(infile, outfile)

