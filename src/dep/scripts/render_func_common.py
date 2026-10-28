"""Shared helpers for the *_func.py render function generators.

Each generator writes two files: the function file given with -o, and a
matching *_table file holding the function pointer table, which the function
file #includes at the end.
"""

import os
import re
import sys


def parse_output_arg(argv):
    # Accepts "-o <file>" and "-o<file>" (case-insensitive), like the perl originals
    if argv and re.match(r'^-o', argv[0], re.I):
        if re.match(r'^-o$', argv[0], re.I):
            if len(argv) > 1:
                return argv[1]
        else:
            return argv[0][2:]
    return None


def open_outputs(argv):
    outfile_fun = parse_output_arg(argv[1:])
    if not outfile_fun:
        sys.exit("Usage: %s -o <output file>\n" % argv[0])

    outfile_tab = re.sub(r'(\.\w+)$', r'_table\1', outfile_fun, flags=re.A)

    print("Generating %s & %s..." % (outfile_fun, outfile_tab))

    fun = open(outfile_fun, 'w', newline='\n')
    tab = open(outfile_tab, 'w', newline='\n')
    return fun, tab, os.path.basename(outfile_tab)
