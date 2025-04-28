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
import markdown
import bbcode_to_markdown


class ClassDoc:
    def __init__(self, name):
        self.name = name
        self.short_description = ""
        self.description = ""
        self.is_experimental = False
        self.is_deprecated = False
        self.deprecated_message = ""
        self.parent_name = ""
        self.children = [] # Array[ClassDoc]
        self.tutorials = [] # Array[TutorialDoc]
        self.is_module = False
        self.xml_file_path = ""
        self.properties = [] # Array[PropertyDoc]
        self.methods = [] # Array[MethodDoc]
        self.signals = [] # Array[SignalDoc]
        self.constants = [] # Array[ConstantDoc]
        self.enums = [] # Array[EnumDoc]
    
    def find_enum(self, name):
        for en in self.enums:
            if en.name == name:
                return en
        return None


class TutorialDoc:
    def __init__(self):
        self.title = ""
        self.link = ""


class ParameterDoc:
    def __init__(self, name):
        self.name = name
        self.type = "" # Variant type, class name, or enum name prefixed with class
        self.default_value = None


class MemberDoc:
    def __init__(self, name):
        self.name = name
        self.description = ""
        self.is_deprecated = False
        self.deprecated_message = ""


class MethodDoc(MemberDoc):
    def __init__(self, name):
        super().__init__(name)
        self.parameters = [] # Array[ParameterDoc]
        self.return_type = "" # Variant type, class name, or enum name prefixed with class
        self.qualifiers = ""


class SignalDoc(MemberDoc):
    def __init__(self, name):
        super().__init__(name)
        self.parameters = [] # Array[ParameterDoc]


class PropertyDoc(MemberDoc):
    def __init__(self, name):
        super().__init__(name)
        self.type = "" # Variant type, class name, or enum name prefixed with class
        self.default_value = None


class ConstantDoc(MemberDoc):
    def __init__(self, name):
        super().__init__(name)
        self.type = ""
        self.value = None


class EnumDoc:
    def __init__(self, name):
        self.name = name
        self.items = [] # Array[ConstantDoc]
    
    def find_item_from_value(self, value):
        for item in self.items:
            if item.value == value:
                return item
        return None


def parse_class_from_xml_tree(xml_tree, xml_file_path): # -> ClassDoc
    root = xml_tree.getroot()

    if root.tag != "class":
        print("Error: No class found. Not a valid Godot class XML file!\n")
        sys.exit(1)

    klass = ClassDoc(root.attrib['name'])
    klass.xml_file_path = xml_file_path
    klass.parent_name = root.attrib['inherits']
    klass.is_module = True
    klass.short_description = root.find('brief_description').text
    klass.description = root.find('description').text

    if 'is_experimental' in root.attrib and root.attrib['is_experimental'] == 'true':
        klass.is_experimental = True
    
    def parse_deprecation(item, xml_node):
        # Godot 4.3 changed `is_deprecated="true"` to `deprecated="message"`
        if 'is_deprecated' in xml_node.attrib and xml_node.attrib['is_deprecated'] == 'true':
            item.is_deprecated = True
        if 'deprecated' in xml_node.attrib:
            item.is_deprecated = True
            item.deprecated_message = xml_node.attrib['deprecated']

    parse_deprecation(klass, root)

    # Tutorials
    tutorials_xml = root.find('tutorials')
    if tutorials_xml is not None:
        links_xml = tutorials_xml.findall('link')
        for link_xml in links_xml:
            tutorial = TutorialDoc()
            tutorial.link = link_xml.text
            tutorial.title = link_xml.attrib['title']
            klass.tutorials.append(tutorial)

    # Properties
    members_container_xml = root.find('members')
    if members_container_xml is not None:
        members_xml = members_container_xml.findall('member')

        for member_xml in members_xml:
            prop = PropertyDoc(member_xml.attrib['name'])
            prop.type = member_xml.attrib['type']
            
            if 'default' in member_xml.attrib:
                prop.default_value = member_xml.attrib['default']
            if 'enum' in member_xml.attrib:
                prop.type = member_xml.attrib['enum']
            
            if member_xml.text is not None:
                prop.description = member_xml.text
            
            parse_deprecation(prop, member_xml)
            
            klass.properties.append(prop)

    def make_params_from_xml(params_xml):
        params = []
        for param_index, param_xml in enumerate(params_xml):
            param = ParameterDoc(param_xml.attrib['name'])
            param.type = param_xml.attrib['type']

            if 'default' in param_xml.attrib:
                param.default_value = param_xml.attrib['default']
            if 'enum' in param_xml.attrib:
                param.type = param_xml.attrib['enum']

            params.append(param)
        return params
    
    # Methods
    methods_container_xml = root.find('methods')
    if methods_container_xml is not None:
        methods_xml = methods_container_xml.findall('method')

        for method_xml in methods_xml:
            method = MethodDoc(method_xml.attrib['name'])

            params_xml = method_xml.findall('param')
            method.parameters = make_params_from_xml(params_xml)

            if 'qualifiers' in method_xml.attrib:
                method.qualifiers = method_xml.attrib['qualifiers']

            return_xml = method_xml.find('return')
            method.return_type = return_xml.attrib['type']
            if 'enum' in return_xml.attrib:
                method.return_type = return_xml.attrib['enum']

            desc_xml = method_xml.find('description')
            if desc_xml is not None:
                method.description = desc_xml.text
            
            parse_deprecation(method, method_xml)

            klass.methods.append(method)

    # Signals
    signals_container_xml = root.find('signals')
    if signals_container_xml is not None:
        signals_xml = signals_container_xml.findall('signal')

        for signal_xml in signals_xml:
            signal = SignalDoc(signal_xml.attrib['name'])

            params_xml = signal_xml.findall('param')
            signal.parameters = make_params_from_xml(params_xml)

            desc_xml = signal_xml.find('description')
            if desc_xml is not None:
                signal.description = desc_xml.text

            parse_deprecation(signal, signal_xml)

            klass.signals.append(signal)

    # Enumerations and constants
    constants_container_xml = root.find('constants')
    if constants_container_xml is not None:
        generic_constants_xml = constants_container_xml.findall('constant')

        enums = {}

        for generic_constant_xml in generic_constants_xml:
            constant = ConstantDoc(generic_constant_xml.attrib['name'])
            constant.value = generic_constant_xml.attrib['value']
            constant.description = generic_constant_xml.text

            parse_deprecation(constant, generic_constant_xml)

            if 'enum' in generic_constant_xml.attrib:
                enum_name = generic_constant_xml.attrib['enum']
                if enum_name in enums:
                    enum = enums[enum_name]
                else:
                    enum = EnumDoc(enum_name)
                    enums[enum_name] = enum
                
                enum.items.append(constant)

            else:
                klass.constants.append(constant)
        
        for enum_name, enum in enums.items():
            klass.enums.append(enum)
    
    return klass


# This is a hack we can do because Markdown allows to fallback on HTML
def make_custom_internal_anchor(name):
    return '<span id="i_' + name + '"></span>'


GODOT_CLASSES_URL = "https://docs.godotengine.org/en/stable/classes"


def get_godot_class_url(name):
    return GODOT_CLASSES_URL + "/class_" + name.lower() + ".html"


def get_godot_member_url(class_name, member_name, member_type):
    # member_type may be:
    # "method"
    # "property"
    # "constant"

    # For some reason there is a special case for members starting with '_'.
    # As a result, we can't link to Godot methods that have both a version with '_' and a version without (like 
    # Object._get and Object.get). In those cases, the link's format is `idN` where N is some kind of 
    # incrementing number... It's hard to figure out automatically. If we have to link these, we'll have to hardcode 
    # every case...
    if member_name.startswith('_'):
        member_name = member_name[1:]

    return get_godot_class_url(class_name) + "#class-" + class_name.lower() + "-" + member_type + "-" \
        + member_name.replace('_', '-')


# Inconsistently different from methods and properties
def get_godot_enum_url(class_name, member_name):
    return get_godot_class_url(class_name) + "#enum-" + class_name.lower() + "-" + member_name.lower()


class ClassFormatter:
    def __init__(self, current_class_name, module_class_names, classes_by_name, links_prefix):
        self.current_class_name = current_class_name # Can be empty
        self.module_class_names = module_class_names
        self.classes_by_name = classes_by_name

        # Prefix to links.
        # in MkDocs, by default, links are relative to the Markdown file in the project structure.
        # It is possible to write a URL relative to the root of the MkDocs project by beginning with a '/'
        # since https://github.com/mkdocs/mkdocs/commit/34ef3ca6d0390959080ce93a695361eea1649272.
        # However, other Markdown renderers might not handle this the same way. For example, when viewed on Github or VSCode's 
        # preview plugin, the "root" is not seen as the top of MkDoc's directory, but the root of the repo, or the root of
        # the workspace. Or it might not be handled at all.
        # So we may have to keep using relative paths and so we have to pass prefixes in some cases.
        self.local_prefix = links_prefix
    

    def make_type(self, name):
        if '.' in name:
            # Assume enum
            parts = name.split('.')
            return self.make_enum_link(parts[0], parts[1])
        if name == "void":
            link = "#"
        elif name in self.module_class_names:
            link = self.local_prefix + name + ".md"
        else:
            link = get_godot_class_url(name)
        return markdown.make_link(name, link)


    def make_enum_link(self, class_name, member_name):
        if class_name in self.module_class_names:
            # TODO Link to specific enum not supported yet
            link = self.local_prefix + class_name + ".md#enumerations"
        else:
            link = get_godot_enum_url(class_name, member_name)
        link_text = member_name
        return markdown.make_link(link_text, link)


    def make_text(self, text):
        return bbcode_to_markdown.format_text(text, self)


    def make_text_single_line(self, text):
        return bbcode_to_markdown.format_text_for_table(text, self)


    def make_default_value(self, value, type_name):
        return make_default_value(value, type_name, self.classes_by_name)


    def make_parameter_list(self, parameters):
        return make_parameter_list(parameters, self.module_class_names, self.current_class_name, self.classes_by_name)


    def make_constants(self, items):
        return make_constants(items, self.module_class_names, self.current_class_name)


    def make_property_link(self, class_name, member_name):
        if class_name in self.module_class_names:
            link = self.local_prefix + class_name + ".md#i_" + member_name
        else:
            link = get_godot_member_url(class_name, member_name, "property")
        if class_name == self.current_class_name:
            link_text = member_name
        else:
            link_text = class_name + "." + member_name
        return markdown.make_link(link_text, link)


    def make_method_link(self, class_name, member_name):
        if class_name in self.module_class_names:
            link = self.local_prefix + class_name + ".md#i_" + member_name
        else:
            link = get_godot_member_url(class_name, member_name, "method")
        if class_name == self.current_class_name:
            link_text = member_name
        else:
            link_text = class_name + "." + member_name
        return markdown.make_link(link_text, link)


    def make_constant_link(self, class_name, member_name):
        if class_name in self.module_class_names:
            # Note, that could either be a constant or an enum member
            link = self.local_prefix + class_name + ".md#i_" + member_name
        else:
            link = get_godot_member_url(class_name, member_name, "constant")
        if class_name == self.current_class_name:
            link_text = member_name
        else:
            link_text = class_name + "." + member_name
        return markdown.make_link(link_text, link)


    def make_signal_link(self, class_name, member_name):
        if class_name in self.module_class_names:
            # TODO Link to specific signals not supported yet
            link = self.local_prefix + class_name + ".md#signals"
        else:
            link = get_godot_member_url(class_name, member_name, "signal")
        link_text = class_name + "." + member_name
        return markdown.make_link(link_text, link)


    # constants: Array[ConstantDoc]
    def make_constants(self, constants):
        s = ""
        for constant in constants:
            s += "- "

            # In Godot docs, both constants and enum items can be referred using `[constant ClassName.ENUM_ITEM]` instead
            # of using the `enum` tag.
            s += make_custom_internal_anchor(constant.name)

            s += "**" + constant.name + "** = **" + constant.value + "**"

            desc = ""
            
            if constant.is_deprecated:
                desc += "*This constant is deprecated."
                if constant.deprecated_message != "":
                    desc += " "
                    desc += constant.deprecated_message
                desc += "*"

            text = constant.description.strip()
            if text != "" or desc != "":
                if desc != "":
                    desc += " "
                desc += self.make_text_single_line(constant.description)

            if desc != "":
                s += " --- "
                s += desc

            s += "\n"

        return s


    # params: Array[ParameterDoc]
    def make_parameter_list(self, params):
        s = "("
        param_index = 0
        for param in params:
            if param_index > 0:
                s += ","
            param_index += 1
            s += " " + self.make_type(param.type) + " " + param.name
            if param.default_value is not None:
                s += "=" + self.make_default_value(param.default_value, param.type)
        s += " )"
        return s


    def make_referred_enum_value(self, value, enum_type):
        parts = enum_type.split('.')
        class_name = parts[0]
        klass = self.classes_by_name.get(class_name, None)
        if klass is None:
            # Can't print a nice symbol. This may be the case for Godot enums, 
            # we'd have to have a representation of all Godot's classes to be able to do this kind of lookup
            return value
        en = klass.find_enum(parts[1])
        if en is None:
            # ?
            return value
        item = en.find_item_from_value(value)
        if item is None:
            # ?
            return value
        return item.name + " (" + value + ")"


    def make_default_value(self, value, type_name):
        if '.' in type_name:
            return self.make_referred_enum_value(value, type_name)
        return value


def make_custom_internal_link(name):
    assert name.find(' ') == -1
    return markdown.make_link(name, "#i_" + name)


def class_doc_to_markdown(
    klass, # ClassDoc
    f_out, # Path to file, or '-' to print the result
    module_class_names, # List of strings
    classes_by_name # {name => ClassDoc}
):
    current_class_name = klass.name

    fmt = ClassFormatter(current_class_name, module_class_names, classes_by_name, '')

    # Header
    out = "# " + current_class_name + "\n\n"
    out += "Inherits: " + fmt.make_type(klass.parent_name) + "\n\n"

    if len(klass.children) > 0:
        links = []
        for child in klass.children:
            links.append(fmt.make_type(child.name))
        out += "Inherited by: " + ', '.join(links) + "\n\n"

    if klass.is_experimental:
        out += ("!!! warning\n    This class is marked as experimental. "
            "It is subject to likely change or possible removal in future versions. Use at your own discretion.")
        out += "\n\n"
    
    if klass.is_deprecated:
        out += "!!! warning\n    This class is deprecated."
        if klass.deprecated_message != "":
            out += " "
            out += fmt.make_text(klass.deprecated_message)
        out += "\n\n"

    class_desc = fmt.make_text(klass.short_description)
    if class_desc != "":
        out += class_desc
        out += "\n\n"

    text = fmt.make_text(klass.description)
    if text.strip() != "":
        out += "## Description: \n\n" + text + "\n\n"

    # Tutorials
    if len(klass.tutorials) > 0:
        out += "## Tutorials: \n\n"
        for tutorial in klass.tutorials:
            out += "- [" + tutorial.title + "](" + tutorial.link + ")\n"

    # Properties summary
    if len(klass.properties) > 0:
        out += "## Properties: \n\n"
        table = [["Type", "Name", "Default"]]
        for prop in klass.properties:
            row = [
                fmt.make_type(prop.type),
                make_custom_internal_link(prop.name)
            ]
            if prop.is_deprecated:
                row[1] += " *(deprecated)*"
            if prop.default_value is not None:
                row.append(fmt.make_default_value(prop.default_value, prop.type))
            else:
                row.append("")
            table.append(row)
        out += markdown.make_table(table)
        out += "\n\n"
        
    # Methods summary
    if len(klass.methods) > 0:
        out += "## Methods: \n\n"
        table = [["Return", "Signature"]]

        # TODO Remove from list if it's a getter/setter of a property
        for method in klass.methods:
            signature = make_custom_internal_link(method.name) + " "
            signature += fmt.make_parameter_list(method.parameters)
            signature += " "

            if method.qualifiers != "":
                signature += method.qualifiers

            row = [
                fmt.make_type(method.return_type),
                signature
            ]

            if method.is_deprecated:
                row[1] += " *(deprecated)*"

            table.append(row)

        out += markdown.make_table(table)
        out += "\n\n"
    
    # Signals
    if len(klass.signals) > 0:
        out += "## Signals: \n\n"

        for signal in klass.signals:
            out += "### "
            out += signal.name

            out += fmt.make_parameter_list(signal.parameters)
            out += " \n\n"

            desc = ""

            if signal.is_deprecated:
                desc += "*This signal is deprecated."
                if signal.deprecated_message != "":
                    desc += " "
                    desc += fmt.make_text(signal.deprecated_message)
                desc += "*\n"

            if signal.description.strip() != "":
                desc += fmt.make_text(signal.description)
            elif not signal.is_deprecated:
                desc += "*(This signal has no documentation)*"
            
            if desc != "":
                out += desc
                out += "\n\n"
    
    # Enums
    if len(klass.enums) > 0:
        out += "## Enumerations: \n\n"

        for enum in klass.enums:
            out += "enum **" + enum.name + "**: \n\n"
            out += fmt.make_constants(enum.items)
            out += "\n"
        
        out += "\n"

    # Constants
    if len(klass.constants) > 0:
        out += "## Constants: \n\n"
        out += fmt.make_constants(klass.constants)
        out += "\n"
    
    # Property descriptions
    if len(klass.properties) > 0:
        out += "## Property Descriptions\n\n"

        for prop in klass.properties:
            out += "### " + fmt.make_type(prop.type) + make_custom_internal_anchor(prop.name) + " **" + prop.name + "**"
            if prop.default_value is not None:
                out += " = " + fmt.make_default_value(prop.default_value, prop.type)
            out += "\n\n"

            if prop.is_deprecated:
                out += "*This property is deprecated."
                if prop.deprecated_message != "":
                    out += " "
                    out += fmt.make_text(prop.deprecated_message)
                out += "*\n"

            if prop.description != "":
                text = fmt.make_text(prop.description)
                if text.strip() != "":
                    out += text
                elif not prop.is_deprecated:
                    out += "*(This property has no documentation)*"
                out += "\n"

            out += "\n"

    # Method descriptions
    if len(klass.methods) > 0:
        out += "## Method Descriptions\n\n"

        for method in klass.methods:
            out += "### " + fmt.make_type(method.return_type) \
                + make_custom_internal_anchor(method.name) + " **" + method.name + "**"
            out += fmt.make_parameter_list(method.parameters)
            out += " "
            if method.qualifiers != "":
                signature += method.qualifiers

            out += "\n\n"

            if method.is_deprecated:
                out += "*This method is deprecated."
                if method.deprecated_message != "":
                    out += " "
                    out += fmt.make_text(method.deprecated_message)
                out += "*\n"

            if method.description != "":
                text = fmt.make_text(method.description)
                if text.strip() != "":
                    out += text
                elif not method.is_deprecated:
                    out += "*(This method has no documentation)*"
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


# Generates a Markdown file containing a list of all the classes, organized in a hierarchy
def generate_classes_index(output_path, classes_by_name, verbose, module_class_names):
    root_classes = []
    for class_name, klass in classes_by_name.items():
        if klass.parent_name == "":
            root_classes.append(klass)

    lines = ["# All classes", ""]
    indent = "    "

    fmt = ClassFormatter('', module_class_names, {}, '')

    def do_branch(lines, classes, level, indent, fmt):
        for klass in classes:
            # TODO Format for abstract classes? XML files don't contain that information...
            link = klass.name
            if klass.is_module:
                link = fmt.make_type(klass.name)
            lines.append(level * indent + "- " + link)
            
            if len(klass.children) > 0:
                do_branch(lines, klass.children, level + 1, indent, fmt)

    for klass in root_classes:
        lines.append("- " + klass.name)
        do_branch(lines, klass.children, 1, indent, fmt)
    
    out = "\n".join(lines)

    if verbose:
        print("Writing", output_path)

    with open(output_path, mode='w', encoding='utf-8') as f:
        f.write(out)


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

    module_class_names = []
    for xml_file in xml_files:
        module_class_names.append(xml_file.stem)
    
    # Parse all XML files
    class_xml_trees = {}
    for src_filepath in xml_files:
        xml_tree = ET.parse(src_filepath)

        root = xml_tree.getroot()
        if root.tag != "class":
            print("Error: No class found in ", src_filepath, "!\n")
            continue

        class_xml_trees[src_filepath] = xml_tree

    # Build class objects
    classes_by_name = {}

    for filename, xml_tree in class_xml_trees.items():
        klass = parse_class_from_xml_tree(xml_tree, filename)
        classes_by_name[klass.name] = klass
            
    # Complete hierarchy with Godot base classes
    # TODO It would be nice to load this from Godot's own XML files?
    godot_classes = [
        ["Object", ""],
        ["Node", "Object"],
        ["RefCounted", "Object"],
        ["Resource", "RefCounted"],
        ["Node3D", "Node"],
        ["RigidBody3D", "Node3D"]
    ]
    for gdclass in godot_classes:
        klass = ClassDoc(gdclass[0])
        klass.parent_name = gdclass[1]
        klass.is_module = False
        classes_by_name[klass.name] = klass

    # Populate children
    for class_name, klass in classes_by_name.items():
        if klass.parent_name != "":
            if klass.parent_name in classes_by_name:
                parent = classes_by_name[klass.parent_name]
                parent.children.append(klass)

    # Sort
    for class_name, klass in classes_by_name.items():
        klass.children.sort(key = lambda x: x.name)

    # Generate Markdown files
    for class_name, klass in classes_by_name.items():
        if klass.is_module:
            dest = dst_dir / (klass.xml_file_path.stem + ".md")
            if verbose:
                print("Converting ", klass.xml_file_path, dest)
            class_doc_to_markdown(klass, dest, module_class_names, classes_by_name)
            count += 1
            doc_files.append(dest)

    generate_classes_index(dst_dir / "all_classes.md", classes_by_name, verbose, module_class_names)
    #count += 1

    print("Generated %d files in %s." % (count, dst_dir))

