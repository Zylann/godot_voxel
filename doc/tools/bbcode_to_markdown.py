#!/usr/bin/python3
# coding: utf-8 

import textwrap
import bbcode
import markdown

# Utilities to convert Godot's documentation BBCode into Markdown


def _extract_member_and_class(symbol, current_class_name):
    member_dot_index = symbol.find('.')
    if member_dot_index != -1:
        class_name = symbol[0 : member_dot_index]
        member_name = symbol[member_dot_index + 1:]
    else:
        if current_class_name is None:
            raise Exception(bb_node.name + \
                " BBCode was used without class name, but there is no current class in this context.")
        class_name = current_class_name
        member_name = symbol
    return class_name, member_name


def format_doc_bbcodes_for_markdown(text, multiline, module_class_names, current_class_name, local_link_prefix):
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
            
            elif bb_node.name == 'member':
                class_name, member_name = _extract_member_and_class(bb_node.get_first_option_key(), current_class_name)
                out += markdown.make_property_link(class_name, member_name, local_link_prefix, module_class_names)

            elif bb_node.name == 'method':
                class_name, member_name = _extract_member_and_class(bb_node.get_first_option_key(), current_class_name)
                out += markdown.make_method_link(class_name, member_name, local_link_prefix, module_class_names)

            elif bb_node.name == 'enum':
                class_name, member_name = _extract_member_and_class(bb_node.get_first_option_key(), current_class_name)
                out += markdown.make_enum_link(class_name, member_name, local_link_prefix, module_class_names)

            elif bb_node.name == 'signal':
                class_name, member_name = _extract_member_and_class(bb_node.get_first_option_key(), current_class_name)
                out += markdown.make_signal_link(class_name, member_name, local_link_prefix, module_class_names)

            elif bb_node.name == 'constant':
                class_name, member_name = _extract_member_and_class(bb_node.get_first_option_key(), current_class_name)
                out += markdown.make_constant_link(class_name, member_name, local_link_prefix, module_class_names)

            elif bb_node.name == 'i':
                # Simple emphasis, usually italic
                out += '*'

            else:
                # Class lookup: assuming name convention, 
                # otherwise we need a complete list of classes and it's a bit cumbersome to obtain
                if bb_node.name[0].isupper():
                    out += markdown.make_type(bb_node.name, local_link_prefix, module_class_names)

                else:
                    # Error fallback
                    print("Unhandled BBCode in Markdown translation:", bb_node.to_string())
                    print(text)
                    out += bb_node.to_string()

    return out


def format_text_for_table(text, module_class_names, current_class_name, local_link_prefix):
    lines = text.splitlines()

    for i in range(0, len(lines)):
        lines[i] = lines[i].strip()

    # Newlines aren't supported, but what to replace them with depends on BBCode
    text = '\n'.join(lines)

    text = format_doc_bbcodes_for_markdown(text, False, module_class_names, current_class_name, local_link_prefix)

    return text


def format_text(text, module_class_names, current_class_name, local_link_prefix):
    text = textwrap.dedent(text)
    md = format_doc_bbcodes_for_markdown(text, True, module_class_names, current_class_name, local_link_prefix)
    # Stripping because due to some newline-related workarounds, we may have introduced extra trailing lines,
    # and there may also be unwanted leading lines. Normally Markdown renderers ignore those, but it's cleaner.
    # Note: we don't use Markdown's indentation syntax (which would break if it begins the text).
    md = md.strip()
    return md

