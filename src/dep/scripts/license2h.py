#!/usr/bin/env python3

# src/license.txt as a C array, for the About box of the SDL2 ImGui interface

import sys

outfile = None
infile = None

args = sys.argv[1:]
i = 0
while i < len(args):
	if args[i] == '-o' and i + 1 < len(args):
		i += 1
		outfile = args[i]
	elif args[i].startswith('-o'):
		outfile = args[i][2:]
	else:
		infile = args[i]
	i += 1

if not (outfile and infile):
	sys.exit("Usage: %s -o <output file> <input file>\n" % sys.argv[0])

print("Generating %s..." % outfile)

with open(infile, 'rb') as fin:
	data = fin.read().replace(b'\r\n', b'\n')

# latin-1 to UTF-8, the text of the interface is UTF-8
try:
	data = data.decode('utf-8').encode('utf-8')
except UnicodeDecodeError:
	data = data.decode('latin-1').encode('utf-8')

with open(outfile, 'w', newline='\n') as fout:
	fout.write("// Generated from src/license.txt by src/dep/scripts/license2h.py, don't edit\n\n")
	fout.write("static const unsigned char LicenseText[] = {\n")
	for n in range(0, len(data), 16):
		fout.write("\t" + "".join("%d," % b for b in data[n:n + 16]) + "\n")
	fout.write("\t0\n};\n")
