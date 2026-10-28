#!/usr/bin/env python3
# Python port of gamelist.pl: scans the burn driver sources and generates
# driverlist.h (and optionally gamelist.txt).

import os
import platform
import re
import sys

# Files are handled as latin-1 so every byte maps to one character, which keeps
# lengths and comparisons identical to the byte-oriented perl version.
ENCODING = 'latin-1'

# perl's lc() on byte strings only folds ASCII
ASCII_LOWER = str.maketrans('ABCDEFGHIJKLMNOPQRSTUVWXYZ', 'abcdefghijklmnopqrstuvwxyz')

DRIVER_RE = re.compile(
	r'\s*(".*"|null|0)\s*,\s*(".*"|null|0)\s*,\s*(".*"|null|0)\s*,\s*(".*"|null|0)\s*,'
	r'\s*(".*"|null|0)\s*,\s*(".*"|null|0)\s*,\s*(".*"|null|0)\s*,\s*(".*"|null|0)\s*,'
	r'\s*(".*"|null|0),\s*(L".*"|null|0)\s*,\s*(L".*"|null|0)\s*,\s*(L".*"|null|0)\s*,'
	r'\s*(L".*"|null)\s*,\s*(\d|[^,]*)',
	re.S | re.I | re.A)

STRUCT_RE = re.compile(r'struct\s+BurnDriver([D|X]?)\s+(\w+)(.*)', re.A)
COMMENT_RE = re.compile(r'(.*)/\*.*?\*/(.*)', re.S)

outfile = None
listfile = None
filelist = []
drivers = {}

# initial values based on column labels length
max_length = [6, 4, 9, 7, 7, 8, 4, 6]


def option_value(args, i, flag):
	# Handles both "-x <value>" and "-x<value>"; returns (value, new index)
	if re.match('^-' + flag + '$', args[i], re.I):
		i += 1
		return (args[i] if i < len(args) else None), i
	return args[i][2:], i


# Process command line arguments
args = sys.argv[1:]
i = 0
while i < len(args):
	arg = args[i]

	if re.match(r'^-o', arg, re.I):
		outfile, i = option_value(args, i, 'o')
	elif re.match(r'^-l', arg, re.I):
		listfile, i = option_value(args, i, 'l')
	elif re.match(r'^-f', arg, re.I):
		# Manifest file: a newline-separated list of input files. Used to pass a
		# large explicit driver-file list without hitting command-line length limits.
		manifest, i = option_value(args, i, 'f')
		with open(manifest, encoding=ENCODING) as f:
			for mline in f:
				mline = mline.strip()
				if mline != '':
					filelist.append(mline)
	elif os.path.isdir(arg):
		for filename in os.listdir(arg):
			if re.search(r'd\w?_.+\.cpp', filename, re.A):
				filelist.append(arg + '/' + filename)
	else:
		filelist.append(arg)
	i += 1

if not (outfile and filelist):
	sys.exit("Usage: %s -o <output file> [-l <gamelist>] <input file|dir> [<input file|dir>...]\n" % sys.argv[0])

print("Generating gamelist in %s..." % outfile)


# The per-line scans below use plain string searches instead of the regexes of
# the perl version (python's regex engine is far too slow for them); the
# results are the same since each line holds a single trailing newline.

def strip_cpp_comment(line):
	# perl: /(.*?)\/\// -> $1
	pos = line.find('//')
	return line[:pos] if pos >= 0 else line


def until_newline(text):
	# what a non-/s ".*" captures
	pos = text.find('\n')
	return text[:pos] if pos >= 0 else text


def strip_quotes(value):
	# Convert NULL/null/0 to empty string or remove quotes
	if re.match(r'^(null|NULL|0)', value):
		return ''
	return re.search(r'(^"?)(.*)\1', value).group(2)


def parse_driver(filename, status, name, struct):
	m = DRIVER_RE.search(struct)
	if not m:
		sys.exit("\nError: Couldn't parse BurnDriver %s in %s" % (name, filename))
	g = m.groups()

	d = [None] * 11
	d[0] = status
	d[1] = g[0]			# Name
	d[2] = g[5]			# Full name
	d[3] = g[6]			# Remarks
	d[4] = g[7]			# Company
	d[5] = g[8]			# Hardware
	d[6] = g[4]			# Year of release
	d[7] = g[1]			# Parent

	# Working status
	if g[13] != "1" and 'BDF_GAME_WORKING' not in g[13]:
		d[8] = "NOT WORKING"
	else:
		d[8] = ""

	parts = filename.split('/')
	sourcefile = parts[-2] + '/' + parts[-1]
	# we still want to remove burn/ from burn/d_parent.cpp
	sourcefile = sourcefile.replace('drv/', '', 1)
	d[9] = sourcefile	# Filename

	for k in range(10):
		d[k] = strip_quotes(d[k])

	# We only want the 1st name
	m = re.search(r'(.*)\\0.*', d[2])
	if m:
		d[2] = m.group(1)

	prototype = 'BDF_PROTOTYPE' in struct
	bootleg = 'BDF_BOOTLEG' in struct
	remarks = d[3] != ""

	d[10] = d[2]
	if prototype or bootleg or remarks:
		d[10] += " ["
	if prototype:
		d[10] += "Prototype"
		if bootleg or remarks:
			d[10] += ", "
	if bootleg:
		d[10] += "Bootleg"
		if remarks:
			d[10] += ", "
	if remarks:
		d[10] += d[3]
	if prototype or bootleg or remarks:
		d[10] += "]"

	# Store max length
	for k in range(1, 8):
		max_length[k] = max(max_length[k], len(d[k]))

	drivers[name] = d


# Build a list of all drivers. Included d_*.h files are appended to filelist
# while it is being walked, so iterate by index.
n = 0
while n < len(filelist):
	filename = filelist[n]
	n += 1

	with open(filename, encoding=ENCODING) as infile:
		lines = iter(infile)
		for line in lines:

			# Strip C++ style // comments
			line = strip_cpp_comment(line)

			# Strip C style /* comments */
			pos = until_newline(line).rfind('/*')
			if pos >= 0:
				temp = line[:pos]
				rest = ''
				while line:
					pos = line.find('*/')
					if pos >= 0:
						rest = until_newline(line[pos + 2:])
						break
					line = next(lines, None)
				line = temp + rest

			# get included .cpp.h files in the same directory
			if '#include' in line and re.match(r'^\s*#include "d_\w*.h"', line, re.A):
				newfile = re.match(r'^\s*#include "(.*)"', line, re.A).group(1)
				m = re.search(r'(.*[/\\])[^/\\]', filename)
				filelist.append((m.group(1) if m else '') + newfile)

			m = 'BurnDriver' in line and STRUCT_RE.search(line)
			if m:
				# We're at the start of a BurnDriver declaration
				status, name, struct = m.group(1), m.group(2), m.group(3)

				# Read the Burndriver structure into a variable
				while True:
					line = next(lines, None)
					if line is None:
						break
					line = strip_cpp_comment(line)
					struct += line
					if ';' in line:
						break

				# Strip anything after the ;
				struct = struct[:struct.rfind(';') + 1]

				# Strip C style /* comments */
				while '/*' in struct:
					m = COMMENT_RE.search(struct)
					if not m:
						break
					struct = m.group(1) + m.group(2)

				# We now have the complete Burndriver structure without comments
				parse_driver(filename, status, name, struct)

# Sort the list of drivers using the decorated game name as the key
# Make the sort order more predictive by adding the driver name
driverlist = sorted(drivers, key=lambda name: (drivers[name][10] + drivers[name][1]).translate(ASCII_LOWER))

out = []
out.append("// This file was generated by %s (python %s)\n" % (sys.argv[0], platform.python_version()))
out.append("\n")
out.append("// Declaration of all drivers\n")
out.append("#define DRV extern struct BurnDriver\n")


def tabs_to_column(column):
	return "\t" * max(0, 8 - column)


debug = False
for name in driverlist:
	d = drivers[name]
	if d[0] == "X":
		out.append("//")
	else:
		if d[0] == "D":
			if not debug:
				debug = True
				out.append("#if defined FBNEO_DEBUG\n")
		elif debug:
			debug = False
			out.append("#endif\n")
		out.append("DRV")
	out.append("\t\t%s;" % name)
	if d[0] in ("D", "X"):
		out.append(tabs_to_column((9 + len(name)) // 4))
		out.append("// ")
		out.append(d[3] if d[3] else "no comment")
		if d[8]:
			out.append(" [%s]" % d[8])
	out.append("\n")
if debug:
	out.append("#endif\n")

out.append("#undef DRV\n")
out.append("\n")
out.append("// Structure containing addresses of all drivers\n")
out.append("// Needs to be kept sorted (using the full game name as the key) to prevent problems with the gamelist in Kaillera\n")
out.append("static struct BurnDriver* pDriver[] = {\n")

debug = False
for name in driverlist:
	d = drivers[name]
	if d[0] == "X":
		out.append("//")
	elif d[0] == "D":
		if not debug:
			debug = True
			out.append("#if defined FBNEO_DEBUG\n")
	elif debug:
		debug = False
		out.append("#endif\n")
	out.append("\t&%s," % name)
	out.append(tabs_to_column((6 + len(name)) // 4))
	out.append("// %s" % d[2])
	if d[0] in ("D", "X"):
		out.append(" [")
		out.append(d[3] if d[3] else "no comment")
		if d[8]:
			out.append(", %s" % d[8])
		out.append("]\n")
	else:
		if d[8]:
			out.append(" [")
			out.append(d[3] if d[3] else "no comment")
			out.append(", %s]" % d[8])
		out.append("\n")
if debug:
	out.append("#endif\n")

out.append("};\n")

out.append("\n")
out.append("// Lookup table containing sourcefiles of all drivers\n")
out.append("struct game_sourcefile_entry {\n")
out.append("\tchar *game_name;\n")
out.append("\tchar *sourcefile;\n")
out.append("};\n")
out.append("\n")
out.append("static game_sourcefile_entry sourcefile_table[] = {\n")

for name in driverlist:
	d = drivers[name]
	if d[1] != "":
		out.append("\t{ \"%s\", \"%s\"},\n" % (d[1], d[9]))

out.append("\t{ \"\\0\", \"\\0\"}\n")
out.append("};\n")

with open(outfile, 'w', encoding=ENCODING, newline='\n') as f:
	f.write(''.join(out))

if listfile:
	print("Generating gamelist.txt...")

	CRLF = "\x0d\x0a"
	# column order in gamelist.txt: (driver field, max_length index, label)
	columns = [(1, 1, "name"), (0, 0, "status"), (2, 2, "full name"), (7, 7, "parent"),
	           (6, 6, "year"), (4, 4, "company"), (5, 5, "hardware"), (3, 3, "remarks")]

	separator = "+" + "+".join("-" * (max_length[m] + 2) for _, m, _ in columns) + "+" + CRLF

	txt = []
	txt.append("This list contains all games supported by FinalBurn Neo." + CRLF + CRLF)
	txt.append("%d games supported in total; " % len(driverlist))
	txt.append("X = excluded from build; ")
	txt.append("D = included in debug build only; ")
	txt.append("NW = not working" + CRLF + CRLF)

	txt.append(separator)
	for _, m, label in columns:
		txt.append("| " + label + " " * (max_length[m] - len(label) + 1))
	txt.append("|" + CRLF)
	txt.append(separator)

	for name in driverlist:
		d = drivers[name]
		for field, m, _ in columns:
			if field == 0:
				status = d[0] if d[0] else " "
				spaces = max_length[0]
				if d[8]:
					status += " NW"
					spaces -= 3
				txt.append("| " + status + " " * max(0, spaces))
			else:
				txt.append("| " + d[field] + " " * max(0, max_length[m] - len(d[field]) + 1))
		txt.append("|" + CRLF)

	txt.append(separator)

	with open(listfile, 'w', encoding=ENCODING, newline='') as f:
		f.write(''.join(txt))
