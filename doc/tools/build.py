#!/usr/bin/python
# coding: utf-8

# Builds Voxel Tools API docs
#
# Requires Python 3.4+
# Run with --help for usage
#
# Configure these variables for your system or specify on the command line
import tempfile
from xmlproc import process_xml
from time import gmtime, strftime
import subprocess
import shutil
import getopt
import glob
import re
import os
from pathlib import Path
import sys


if sys.version_info < (3, 4):
    print("Please upgrade python to version 3.4 or higher.")
    print("Your version: %s\n" % sys.version_info)
    sys.exit(1)


def generate_api_docs_from_classes_xml(xml_path, output_path, verbose=False):

    # Make output dir and remove old files
    if not os.path.isdir(output_path):
        if verbose:
            print("Making output directory: " + output_path)
        os.makedirs(output_path)

    for i in output_path.glob("*.md"):
        if verbose:
            print("Removing old: ", i)
        os.remove(i)

    # Convert files to MD
    xml_files = xml_path.glob("*.xml")
    doc_files = []
    count = 0
    for src in xml_files:
        dest = output_path / (src.stem + ".md")
        if verbose:
            print("Converting ", src, dest)
        process_xml(src, dest)
        count += 1
        doc_files.append(dest)

    # Create Class index
    index_path = output_path / "Class_List.md"
    doc_files = sorted(doc_files)

    str = "# Voxel Tools Class List\n\n"
    str += "These classes are exposed to GDScript. This information is also available within the Godot script editor by searching help.\n\n"

    for i in doc_files:
        f = Path(i).name
        str += "* [%s](%s)\n" % (f, f)

    str += "\n---\n"
    str += "* [Doc Index](../01_get-started.md)\n"
    str += "_Generated on " + strftime("%b %d, %Y", gmtime()) + "_\n"

    if verbose:
        print("Writing %s" % index_path)
    outfile = open(index_path, 'a')
    outfile.write(str)
    count += 1

    print("Generated %d files in %s." % (count, output_path))


def update_classes_xml(godot_executable, path, verbose=False):

    # Dump XML files from Godot
    args = [godot_executable, ' --doctool ',  path]
    if verbose:
        print("Running: ", args)
    result = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            universal_newlines=True)
    if verbose:
        print(result.stdout)
        print("Disregard Godot's errors about files unless they are about Voxel*.")


def print_usage():
    print("\nUsage: ", sys.argv[0],
          "[-v] [-u] [-g path_to_godot] [-o output_dir]")
    print()
    print("\t-v -> Verbose")
    print("\t-u -> Execute doctool to update XML class data")
    print()


def find_godot(godot, bindir):

    # If godot was specified or hard coded, verify file
    if godot != "":
        if os.path.isfile(godot):
            return godot
        else:
            print("Error: %s is not a valid file." % godot)
            return None

    # Otherwise search for it

    # Match a filename like these
    # godot.windows.opt.tools.64.exe
    # godot.x11.opt.tools.64
    #regex = r"godot\.(windows|x11|osx)(\.opt)?(\.tools)?\.(32|64)(\.exe)?"
    if sys.platform == "win32" or sys.platform == "cygwin":
        filemask = 'godot.*.exe'
    else:
        filemask = 'godot.*.[36][24]'

    binfiles = glob.glob(bindir+filemask)
    if len(binfiles) > 0:
        return binfiles[0]

    print("Error: Godot binary not specified and none found in %s" % bindir)
    return None


###########################
# Main()

def main():
    # Default paths are determined from the location of this script
    my_path = Path(os.path.realpath(__file__))

    # Default parameters
    verbose = False
    run_doctool = False
    godot_repo_root = my_path.parents[4]
    bindir = godot_repo_root / 'bin'
    godot_executable = ""
    output_path = my_path.parents[1] / 'api'
    xml_path = my_path.parents[1] / 'classes'

    # Parse command line arguments
    try:
        opts, args = getopt.getopt(sys.argv[1:], "hvug:o:", "help")
    except getopt.error as msg:
        print("Error: ", msg)
        print_usage()

    for opt, arg in opts:
        if opt in ('-h', '--help'):
            print_usage()
        if opt == '-g':
            godot_executable = arg
        if opt == '-o':
            output_path = Path(arg)
        if opt == '-v':
            verbose = True
        if opt == '-u':
            run_doctool = True

    if run_doctool:
        maybe_godot_executable = find_godot(godot_executable, bindir)
        if maybe_godot_executable is None:
            print_usage()
            return
        else:
            godot_executable = maybe_godot_executable

        if verbose:
            print("Found Godot at: %s" % godot_executable)

        update_classes_xml(godot_executable, godot_repo_root, verbose)

    generate_api_docs_from_classes_xml(xml_path, output_path, verbose)


# If called from command line
if __name__ == "__main__":
    main()
