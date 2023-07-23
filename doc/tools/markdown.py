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


def make_type(name, local_prefix, module_class_names):
    if name == "void":
        link = "#"
    elif name in module_class_names:
        link = local_prefix + name + ".md"
    else:
        link = GODOT_CLASSES_URL + "/class_" + name.lower() + ".html"
    return make_link(name, link)


# TESTING
# if __name__ == "__main__":
#     out = make_table([["Name", "Description"], ["One", "Description 1"], ["Two", "Description 2"]])
#     print(out)
