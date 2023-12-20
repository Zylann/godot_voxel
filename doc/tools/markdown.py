#!/usr/bin/python3
# coding: utf-8 

# Utilities to write Markdown docs.
# No reference to XML stuff here.

GODOT_CLASSES_URL = "https://docs.godotengine.org/en/stable/classes"


def make_link(text, url):
    return "[" + text + "](" + url + ")"


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


def get_godot_class_url(name):
    return GODOT_CLASSES_URL + "/class_" + name.lower() + ".html"


# Note: in MkDocs, by default, links are relative to the Markdown file in the project structure.
# It is possible to write a URL relative to the root of the MkDocs project by beginning with a '/'
# since https://github.com/mkdocs/mkdocs/commit/34ef3ca6d0390959080ce93a695361eea1649272.
# However, other Markdown renderers might not handle this the same way. For example, when viewed on Github or VSCode's 
# preview plugin, the "root" is not seen as the top of MkDoc's directory, but the root of the repo, or the root of
# the workspace. Or it might not be handled at all.
# So we may have to keep using relative paths and so we have to pass prefixes in some cases.

def make_type(name, local_prefix, module_class_names):
    if name == "void":
        link = "#"
    elif name in module_class_names:
        link = local_prefix + name + ".md"
    else:
        link = get_godot_class_url(name)
    return make_link(name, link)


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


def make_property_link(class_name, member_name, local_prefix, module_class_names):
    if class_name in module_class_names:
        link = local_prefix + class_name + ".md#i_" + member_name
    else:
        link = get_godot_member_url(class_name, member_name, "property")
    link_text = class_name + "." + member_name
    return make_link(link_text, link)


def make_method_link(class_name, member_name, local_prefix, module_class_names):
    if class_name in module_class_names:
        link = local_prefix + class_name + ".md#i_" + member_name
    else:
        link = get_godot_member_url(class_name, member_name, "method")
    link_text = class_name + "." + member_name
    return make_link(link_text, link)


def make_enum_link(class_name, member_name, local_prefix, module_class_names):
    if class_name in module_class_names:
        # TODO Link to specific enum not supported yet
        link = local_prefix + class_name + ".md#enumerations"
    else:
        link = get_godot_enum_url(class_name, member_name)
    link_text = class_name + "." + member_name
    return make_link(link_text, link)


def make_constant_link(class_name, member_name, local_prefix, module_class_names):
    if class_name in module_class_names:
        # Note, that could either be a constant or an enum member
        link = local_prefix + class_name + ".md#i_" + member_name
    else:
        link = get_godot_member_url(class_name, member_name, "constant")
    link_text = class_name + "." + member_name
    return make_link(link_text, link)


def make_signal_link(class_name, member_name, local_prefix, module_class_names):
    if class_name in module_class_names:
        # TODO Link to specific signals not supported yet
        link = local_prefix + class_name + ".md#signals"
    else:
        link = get_godot_member_url(class_name, member_name, "signal")
    link_text = class_name + "." + member_name
    return make_link(link_text, link)


# TESTING
# if __name__ == "__main__":
#     out = make_table([["Name", "Description"], ["One", "Description 1"], ["Two", "Description 2"]])
#     print(out)
