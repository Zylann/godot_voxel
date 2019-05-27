#!/usr/bin/python
# coding: utf-8

# Builds Voxel Tools GDScript API
#
# Requires Python 3.4+
# Run with --help for usage
#
# Configure these variables for your system or specify on the command line
OUT="../api"	    # Path to output the gdscript files  
GD=""               # Path to Godot binary, specify on command line, or we will search


import sys
if(sys.version_info<(3,4)):
    print("Please upgrade python to version 3.4 or higher.")
    print("Your version: %s\n" %sys.version_info)
    sys.exit(1)

from pathlib import Path
import os
import re
import glob
import getopt
import shutil
import tempfile 
import subprocess
from time import gmtime, strftime
from xmlproc import process_xml



def build_gdscript_api(verbose=False):

        # Make temp dirs
        tmp_d = tempfile.mkdtemp()
        if verbose: 
            print ("Making temp directories in %s" % tmp_d)
        os.makedirs(tmp_d + "/doc/classes")
        os.makedirs(tmp_d + "/modules/voxel/doc_classes")   
        # Last is not currently used, but will be once VT starts using doc_classes. 


        # Make output dir and remove old files
        if not os.path.isdir(OUT):  
            if verbose: 
                print ("Making output directory: " + OUT)
            os.makedirs(OUT)

        for i in glob.glob(OUT + "*.md"):
            if verbose: 
                print ("Removing old: ", i)
            os.remove(i)


	# Dump XML files from Godot
        args = [GD, ' --doctool ',  tmp_d ]
        if verbose:
            print ("Running: ", args)
        result = subprocess.run (args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, 
                universal_newlines=True)
        if verbose: 
            print (result.stdout)
            print("Disregard Godot's errors about files unless they are about Voxel*.")


	# Convert files to MD
        files = glob.glob(tmp_d + "/doc/classes/Voxel*.xml")
        files += glob.glob(tmp_d + "/modules/voxel/doc_classes/*.xml")
        count=0
        for src in files:
            dest = OUT + Path(src).stem + ".md"
            if verbose: 
                print("Converting ", src, dest)
            process_xml(src, dest)
            count += 1


        # Create Class index
        index=OUT + "Class_List.md"
        files = sorted(glob.glob(OUT+"Voxel*.md"))

        str  = "# Voxel Tools Class List\n\n" 
        str += "These classes are exposed to GDScript. This information is also available within the Godot script editor by searching help.\n\n"

        for i in files:
            f = Path(i).name
            str += "* [%s](%s)\n" % (f, f)

        str += "\n---\n"
        str += "* [Doc Index](../01_get-started.md)\n"
        str += "_Generated on " + strftime("%b %d, %Y", gmtime()) + "_\n" 

        if verbose: 
            print("Writing %s" % index)
        outfile = open(index, 'a')
        outfile.write(str)
        count += 1


	# Remove temporary files
        if verbose: 
            print("Removing temp directory %s" % tmp_d)
        shutil.rmtree (tmp_d, ignore_errors=True)


        # Finished
        print ("Generated %d files in %s." % (count, OUT))



def print_usage():
    print("\nUsage: ", sys.argv[0], "[-v] [-g path_to_godot] [-o output_dir]");
    print("\t-v -> Verbose");
    print()
    print("Current output directory: %s" %OUT);
    print()
    sys.exit(0)



def find_godot(godot):

    # If godot was specified or hard coded, verify file
    if godot != "":
        if os.path.isfile(godot):
            return godot
        else:                   
            print ("Error: %s is not a valid file." %godot)
            print_usage()

    # Otherwise search for it

    # Match a filename like these
    #godot.windows.opt.tools.64.exe
    #godot.x11.opt.tools.64
    #regex = r"godot\.(windows|x11|osx)(\.opt)?(\.tools)?\.(32|64)(\.exe)?"
    bindir='../../../../bin/'
    if sys.platform == "win32" or sys.platform == "cygwin":
        filemask= 'godot.*.exe'
    else:
        filemask = 'godot.*.[36][24]'

    binfiles = glob.glob(bindir+filemask)
    if len(binfiles)>0:
        return binfiles[0]


    print ("Error: Godot binary not specified and none found in %s" %bindir)
    print_usage()




###########################
## Main()

def main():
    pass


# If called from command line
if __name__ == "__main__":

    verbose = False

    try: 
        opts, args = getopt.getopt(sys.argv[1:], "hvg:o:", "help")
    except getopt.error as msg: 
        print ("Error: ", msg )
        print_usage()

    for opt, arg in opts:
        if opt in ('-h', '--help'): 
            print_usage()
        if opt == '-g': 
            GD = arg
        if opt == '-o': 
            OUT = arg
        if opt == '-v': 
            verbose = True

   # Ensure output directory has a trailing slash
    if OUT[-1:] != '/': 
        OUT += '/'

    GD = find_godot(GD)

    if verbose: 
        print ("Found Godot at: %s" %GD)

    # Finished GD and directory checks. Build the files.
    build_gdscript_api(verbose)


