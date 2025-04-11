#!/usr/bin/python3
# coding: utf-8

from pathlib import Path
import os


def get_icons(repo_dir):
    icons_path = repo_dir / Path("project/addons/zylann.voxel/editor/icons")
    icons = list(icons_path.glob("*.svg"))
    return icons


def generate_icon_entries(gdextension_config_path, icons):
    icon_lines = []
    for icon in icons:
        line = icon.stem + " = \"./editor/icons/" + icon.name + "\"\n"
        icon_lines.append(line)

    with open(gdextension_config_path, "r") as f:
        src_lines = f.readlines()

    dst_lines = []
    in_generated_section = False

    for src_line_index in range(0, len(src_lines)):
        line = src_lines[src_line_index]

        if in_generated_section:
            if "</generated" in line:
                in_generated_section = False
            else:
                continue
        
        dst_lines.append(line)

        if "<generated-icons>" in line:
            dst_lines.append("")
            dst_lines += icon_lines
            dst_lines.append("")
            in_generated_section = True
    
    with open(gdextension_config_path, "w", newline='\n') as f:
        f.writelines(dst_lines)


def main():
    my_path = Path(os.path.realpath(__file__))
    repo_dir = my_path.parents[1]

    icons = get_icons(repo_dir)

    generate_icon_entries(repo_dir / Path("project/addons/zylann.voxel/voxel.gdextension"), icons)
    generate_icon_entries(repo_dir / Path("project/addons/zylann.voxel/voxel.gdextension-release"), icons)


main()
