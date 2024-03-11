#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Utility to patch Godot on CI with profiling instrumentation.

import json
import os
import sys


class Instrument:
    # Insert instrument at the beginning of the matched line
    MODE_PREPEND = 0
    # Insert instrument at the end of the matched line
    MODE_APPEND = 1
    # Wraps the matched line in braces and inserts instrument at the beginning
    MODE_WRAP = 2
    # Insert instrument at the beginning of the line following the matched line
    MODE_PREPEND_NEXT_LINE = 3

    def __init__(self):
        # Default mode.
        # Most of the time we want to instrument whole functions. Code style conveniently puts opening brace at the end 
        # of the signature, so we can just append it there by default. This is also a better option in case the 
        # function has a preprocessor directive at its beginning.
        self.mode = self.MODE_APPEND
        # self.regex = ""
        # Match lines containing this text
        self.match_text = ""
        # Custom markup to insert
        self.markup = ""
        self.allow_multiple = False


class FileToInstrument:
    def __init__(self):
        self.instruments = []


class Settings:
    def __init__(self):
        self.files = {}
        # Long include path so we don't have to modify Godot's core build script
        self.header_include = "#include \"thirdparty/tracy/public/tracy/Tracy.hpp\""
        self.profiling_scope = "ZoneScoped;"
    
    def load_from_file(self, settings_fpath):
        with open(settings_fpath, 'r', encoding='UTF-8') as settings_file:
            settings_json_data = json.load(settings_file)
        
        files_data = settings_json_data['files']

        for fpath, file_data in files_data.items():
            if fpath in self.files:
                raise Exception("Duplicate file?")
            
            fti = FileToInstrument()
            self.files[fpath] = fti

            if type(file_data) == list:
                instruments_data = file_data

                for instrument_data in instruments_data:
                    instrument = Instrument()

                    if type(instrument_data) == str:
                        # Default mode
                        instrument.match_text = instrument_data

                    elif type(instrument_data) == dict:
                        # Explicit mode
                        instrument.match_text = instrument_data['match_text']
                        mode_str = instrument_data['mode']

                        if 'allow_multiple' in instrument_data:
                            instrument.allow_multiple = instrument_data['allow_multiple']
                        
                        if 'markup' in instrument_data:
                            instrument.markup = instrument_data['markup']

                        if mode_str == 'prepend':
                            instrument.mode = Instrument.MODE_PREPEND

                        elif mode_str == 'append':
                            instrument.mode = Instrument.MODE_APPEND

                        elif mode_str == 'wrap':
                            instrument.mode = Instrument.MODE_WRAP

                        elif mode_str == 'prepend_next_line':
                            instrument.mode = Instrument.MODE_PREPEND_NEXT_LINE

                        else:
                            raise Exception("Unknown instrument mode '{}'".format(mode_str))

                    else:
                        raise Exception("Unexpected instrument format")

                    fti.instruments.append(instrument)
            else:
                raise Exception("Unexpected file format")


def find_first_line_beginning_with(lines, pattern):
    for i in range(0, len(lines)):
        line = lines[i]
        if line.startswith(pattern):
            return i
    return -1


# def get_indentation(line):
#     return line[: len(line) - line.lstrip(line)]


def apply_instrument(instrument, lines, start_line_index, settings, fpath):
    matched_line_indices = []
    valid = True

    for line_index in range(start_line_index, len(lines)):
        line = lines[line_index]
        if instrument.match_text in line:
            if len(matched_line_indices) > 0 and not instrument.allow_multiple:
                print("ERROR: instrument is matching more than once.")
                print("    In file: {}".format(fpath))
                print("    Pattern: {}".format(instrument.match_text))
                valid = False
                break
            matched_line_indices.append(line_index)
    
    if not valid:
        return False
    
    if len(matched_line_indices) == 0:
        print("ERROR: instrument could not find a match.")
        print("    In file: {}".format(fpath))
        print("    Pattern: {}".format(instrument.match_text))
        return False
    
    if instrument.markup != "":
        markup = instrument.markup
    else:
        markup = settings.profiling_scope

    for matched_line_index in matched_line_indices:
        # Note: modifications should not add or remove lines to preserve line numbers,
        # so indices remain valid

        if instrument.mode == Instrument.MODE_PREPEND:
            line = lines[matched_line_index]
            lstripped_line = line.lstrip()
            indentation = line[:len(line) - len(lstripped_line)]
            lines[matched_line_index] = "{}{} {}".format(indentation, markup, lstripped_line)

        elif instrument.mode == Instrument.MODE_APPEND:
            line = lines[matched_line_index]
            # Use rstrip to remove the original newline
            lines[matched_line_index] = "{} {}\n".format(line.rstrip(), markup)

        elif instrument.mode == Instrument.MODE_PREPEND_NEXT_LINE:
            line = lines[matched_line_index + 1]
            lstripped_line = line.lstrip()
            indentation = line[:len(line) - len(lstripped_line)]
            lines[matched_line_index + 1] = "{}{} {}".format(indentation, markup, lstripped_line)
        
        elif instrument.mode == Instrument.MODE_WRAP:
            line = lines[matched_line_index]
            lstripped_line = line.lstrip()
            indentation = line[:len(line) - len(lstripped_line)]
            # Use rstrip to remove the original newline because we have to add a brace after it
            lines[matched_line_index] = "{}{{ {} {} }}\n".format(indentation, markup, lstripped_line.rstrip())
    
    return True


def run(root_dir, settings_fpath):
    settings = Settings()
    settings.load_from_file(settings_fpath)

    # Instead of interrupting everything on the first error, go through all files so we can get them all
    error_count = 0

    for relative_fpath, fti in settings.files.items():
        print("Instrumenting file {}...".format(relative_fpath))

        fpath = os.path.join(root_dir, relative_fpath)

        # Read file
        lines = []
        try:
            with open(fpath, 'r', encoding='UTF-8') as f:
                for line in f:
                    lines.append(line)
        except OSError as ex:
            print("ERROR: cannot open file {}".format(fpath))
            error_count += 1
            continue

        # Insert header include

        line_index = find_first_line_beginning_with(lines, '#')

        if line_index == -1:
            print("ERROR: cannot find beginning of #includes in {}".format(fpath))
            error_count += 1
            continue
        
        # TODO Find a way to re-use an empty line so line numbers are not affected
        lines.insert(line_index, settings.header_include + '\n')
        start_line_index = line_index + 1

        # Insert instrumentations
        for instrument in fti.instruments:
            if not apply_instrument(instrument, lines, start_line_index, settings, fpath):
                error_count += 1
        
        if error_count == 0:
            # Write file
            with open(fpath, 'w', encoding='UTF-8', newline='\n') as f:
                f.writelines(lines)
    
    print("Done with", error_count, "errors")
    return error_count


def main():
    args = sys.argv[1:]

    if len(args) != 4:
        print("\nUsage: ", sys.argv[0], "-d \"source dir\" -c \"config path\"")
        print()
        print("\t-d -> Specifies the directory in which the source code to instrument is located")
        print("\t-c -> Specifies the JSON configuration file containing which instrumentations to do")
        return
    
    source_dir = "."
    config_path = "instruments.json"

    i = 0
    while i < len(args):
        if args[i] == '-d':
            i += 1
            config_path = args[i]
        
        elif args[i] == '-c':
            i += 1
            config_path = args[i]
        
        i += 1

    error_count = run(source_dir, config_path)
    if error_count > 0:
        return 1
    return 0


if __name__ == "__main__":
    code = main()
    # Signaling errors like this allows our CI to pick them up and cancel jobs
    sys.exit(code)

