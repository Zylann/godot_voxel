#!/usr/bin/python3
# coding: utf-8

# Small BBCode parser for reading Godot's XML doc format, so it can be translated into different formats (not just
# Markdown, HTML etc).


class NodeText:
    def __init__(self, text):
        self.text = text


class NodeTag:
    def __init__(self):
        self.name = ""
        self.value = ""
        self.options = {}
        self.closing = False

    def to_string(self):
        out = ""
        out += "["
        if self.closing:
            out += '/'
        out += self.name
        if self.value != "":
            out += "=" + self.value
        for key, value in self.options.items():
            out += " " + key
            if value != "":
                out += "=" + value
        out += "]"
        return out
    
    def is_opening(self):
        return not self.closing
    
    def get_first_option_key(self):
        for key in self.options:
            return key
        raise Exception("The tag has no options")


def find_next_unescaped_bracket(text, from_pos):
    pos = from_pos
    while pos < len(text):
        pos = text.find("[", pos)
        if pos == -1:
            return -1
        if pos > 0 and text[pos - 1] == '\\':
            pos += 1
            continue
        return pos
    return -1


def is_name_start(c):
    return c.isalpha() or c == '_'


def is_name_part(c):
    return c.isalnum() or c == '_' or c == '.'


def parse_name(text, begin_pos): # -> name
    pos = begin_pos
    name = ""
    while pos < len(text):
        c = text[pos]
        if is_name_part(c):
            pos += 1
            continue
        break
    return text[begin_pos : pos]


def parse_tag_option_value(text, begin_pos): # -> value
    pos = begin_pos
    while pos < len(text):
        c = text[pos]
        if c == ']':
            break
        if c == ' ':
            break
        pos += 1
    return text[begin_pos : pos]


def parse_tag_option(text, begin_pos): # -> name?, value?, pos
    if not is_name_start(text[begin_pos]):
        return None, None, begin_pos
    
    pos = begin_pos

    name = parse_name(text, pos)
    pos += len(name)

    if pos >= len(text):
        return None, None, pos
    
    value = ""

    c = text[pos]
    if c == '=':
        pos += 1
        if pos >= len(text):
            return None, None, pos
        value = parse_tag_option_value(text, pos)
        pos += len(value)
    
    return name, value, pos


def parse_tag(text, pos, nodes): # -> success, end_pos
    if text[pos] == '/':
        # End tag
        pos += 1
        if pos >= len(text):
            return False, pos
        tag = NodeTag()
        tag.name = parse_name(text, pos)
        tag.closing = True
        pos += len(tag.name)
        if pos >= len(text):
            return False, pos
        if text[pos] != ']':
            return False, pos
        nodes.append(tag)
        pos += 1
        return True, pos

    # Tag name and optional value
    key, value, pos = parse_tag_option(text, pos)
    if key is None:
        return False, pos + 1
    
    tag = NodeTag()
    tag.name = key
    tag.value = value

    while pos < len(text):
        c = text[pos]

        if c == ' ':
            pos += 1
            continue
        
        if is_name_start(c):
            key, value, pos = parse_tag_option(text, pos)
            if key is None:
                # Option without value
                return False, pos + 1
            tag.options[key] = value
            continue

        if c == ']':
            nodes.append(tag)
            return True, pos + 1
        
        raise Exception("Unexpected character '" + c + "'")
    
    return False, pos


def unescape_text(text):
    return text.replace("\\[", '[')


def parse_text(text, begin_pos, nodes): # -> pos
    pos = begin_pos

    while (pos < len(text)):
        open_bracket_pos = find_next_unescaped_bracket(text, pos)

        if open_bracket_pos != -1:
            if open_bracket_pos > pos:
                nodes.append(NodeText(unescape_text(text[pos : open_bracket_pos])))

            success, end_pos = parse_tag(text, open_bracket_pos + 1, nodes)
            pos = end_pos
            if not success:
                # print("ERROR, parse_tag didn't succeed")
                # Append failed tag as text
                nodes.append(NodeText(text[open_bracket_pos : end_pos]))
        
        else:
            # No remaining tags
            nodes.append(NodeText(unescape_text(text[pos:])))
            break

    return pos


# Parses text into a list of nodes, which can either be text or tags.
def parse(text): # -> nodes
    nodes = []
    parse_text(text, 0, nodes)
    return nodes


# Debug
def print_nodes_as_list(nodes):
    for node in nodes:
        if isinstance(node, NodeText):
            print("Text:`" + node.text + "`")
        elif isinstance(node, NodeTag):
            print("Tag:`" + node.name + "`")
        else:
            print("<error>", node)


# Debug
def get_nodes_as_bbcode(nodes):
    out = ""
    for node in nodes:
        if isinstance(node, NodeText):
            out += node.text
        elif isinstance(node, NodeTag):
            out += node.to_string()
    return out


# Testing
if __name__ == "__main__":
    text = "Outputs values from the custom input having the same name as the node. May be used in [VoxelGraphFunction]. It won't be used in [VoxelGeneratorGraph]."
    nodes = parse(text)
    print(print_nodes_as_list(nodes))

    # text = ("Hello World, [i]Text[/i] is [b][i]Fun[/i][/b], with [ClassName], "
    #     "[color=#123456aa]Our [member Yolo.jesus][/color] and "
    #     "[url=xxx]link[/url], interval [0..1] in code is [code][0..1][/code]")
    # nodes = parse(text)
    # bb = get_nodes_as_bbcode(nodes)
    # print("Equals: ", bb == text)
    # print(bb)
    # print(text)

