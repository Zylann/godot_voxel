#!/usr/bin/python3
# coding: utf-8

# Builds Voxel Tools API docs
#
# Requires Python 3.4+
# Run with --help for usage
#
# Configure these variables for your system or specify on the command line

import sys

if sys.version_info < (3, 4):
    print("Please upgrade python to version 3.4 or higher.")
    print("Your version: %s\n" % sys.version_info)
    sys.exit(1)

import xml_to_markdown
import subprocess
import getopt
import glob
import os
from pathlib import Path

SOURCES = "source"


def update_classes_xml(custom_godot_path, godot_repo_root, verbose=False):
    godot_executable = custom_godot_path
    if godot_executable is None or godot_executable == "":
        bindir = godot_repo_root / 'bin'
        godot_executable = find_godot(bindir)
        if godot_executable is None:
            print("Godot executable not found")
            return

    if verbose:
        print("Found Godot at: %s" % godot_executable)
    
    # Dump XML files from Godot
    args = [str(godot_executable), ' --doctool ', str(godot_repo_root)]
    if verbose:
        print("Running: ", args)
    result = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            universal_newlines=True)
    if verbose:
        print(result.stdout)
        print("Disregard Godot's errors about files unless they are about Voxel*.")


def find_godot(bindir):
    # Match a filename like these
    # godot.windows.tools.64.exe
    # godot.x11.tools.64
    #regex = r"godot\.(windows|x11|osx)(\.opt)?(\.tools)?\.(32|64)(\.exe)?"
    prefix = "godot"
    suffix = ""
    if sys.platform == "win32" or sys.platform == "cygwin":
        prefix += ".windows"
        suffix = ".exe"
    else:
        # TODO Mac OS?
        prefix += ".x11"

    bits = ".64"

    # Names to try by priority
    names = [
        prefix + ".tools" + bits + suffix,
        prefix + ".opt.tools" + bits + suffix
    ]

    for name in names:
        path = bindir / name
        if os.path.isfile(path):
            return path

    print("Error: Godot binary not specified and none suitable found in %s" % bindir)
    return None


def update_mkdocs_file(mkdocs_config_fpath, md_classes_dir):
    absolute_paths = md_classes_dir.glob("*.md")
    class_files = []
    docs_folder = mkdocs_config_fpath.parents[0] / SOURCES
    for absolute_path in absolute_paths:
        class_files.append(str(absolute_path.relative_to(docs_folder)).replace('\\', '/'))
    class_files = sorted(class_files)

    with open(mkdocs_config_fpath, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    processed_lines = []
    in_generated_section = False

    for line in lines:
        if in_generated_section:
            if "</generated_class_list>" in line:
                in_generated_section = False
            else:
                continue
        if "<generated_class_list>" in line:
            in_generated_section = True
            indent = line[:line.find('#')]
            processed_lines.append(line)
            for class_file in class_files:
                processed_lines.append(indent + "- " + class_file + "\n")
        else:
            processed_lines.append(line)

    yml = "".join(processed_lines)
    with open(mkdocs_config_fpath, 'w', encoding='utf-8') as f:
        f.write(yml)


def print_usage():
    print("\nUsage: ", sys.argv[0],
          "[-d] [-a] [-m] [-h] [-v] [-g path_to_godot]")
    print()
    print("\t-d -> Execute Godot doctool to update XML class data")
    print("\t-a -> Update Markdown API files from XML class data")
    print("\t-m -> Update mkdocs config file with generated content such as API files")
    print("\t-h, --help -> Prints help")
    print("\t-v -> Verbose. Print more details when running.")
    print("\t-g -> Specify custom path to Godot. Otherwise will use compiled executable under the repo's bin directory.")
    print()


###########################
# Main()

def main():
    # Default paths are determined from the location of this script
    my_path = Path(os.path.realpath(__file__))

    # Default parameters
    verbose = False
    must_run_doctool = False
    must_update_md_from_xml = False
    must_update_mkdocs_config = False
    godot_executable = ""

    godot_repo_root = my_path.parents[4]
    md_path = my_path.parents[1] / SOURCES / 'api'
    xml_path = my_path.parents[1] / 'classes'
    mkdocs_config_path = my_path.parents[1] / 'mkdocs.yml'

    # Parse command line arguments
    try:
        opts, args = getopt.getopt(sys.argv[1:], "damhvg:", "help")
    except getopt.error as msg:
        print("Error: ", msg)
        print_usage()
        return

    did_something = False

    for opt, arg in opts:
        if opt == '-d':
            must_run_doctool = True
        if opt == '-a':
            must_update_md_from_xml = True
        if opt == '-m':
            must_update_mkdocs_config = True
        if opt in ('-h', '--help'):
            print_usage()
            did_something = True
        if opt == '-v':
            verbose = True
        if opt == '-g':
            godot_executable = arg

    if must_run_doctool:
        update_classes_xml(godot_executable, godot_repo_root, verbose)
        did_something = True
    
    if must_update_mkdocs_config:
        update_mkdocs_file(mkdocs_config_path, md_path)
        did_something = True

    if must_update_md_from_xml:
        xml_to_markdown.process_xml_folder(xml_path, md_path, verbose)
        did_something = True

    if not did_something:
        print("No operation specified.")
        print_usage()


# If called from command line
if __name__ == "__main__":
    main()
