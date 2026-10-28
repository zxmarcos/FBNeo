#!/usr/bin/env python3

import re
import sys

outfile = None
infile = None

font = "Tahoma"
font_size = "16"

escape = False


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
	if re.match(r'^-o', args[i], re.I):
		outfile, i = option_value(args, i, 'o')			# Output file
	elif re.match(r'^-f', args[i], re.I):
		font, i = option_value(args, i, 'f')			# Font
	elif re.match(r'^-s', args[i], re.I):
		font_size, i = option_value(args, i, 's')		# Font size
	elif re.match(r'^-esc', args[i], re.I):
		escape = True
	else:
		infile = args[i]
	i += 1

if not (outfile and font and font_size and infile):
	sys.exit("Usage: %s -o <output file> <input file> [-f <font> -s <font-size> -escape]\n" % sys.argv[0])

print("Generating %s..." % outfile)

with open(infile, encoding='latin-1') as fin, open(outfile, 'w', encoding='latin-1', newline='') as fout:
	fout.write("{\\rtf1\\ansi\\ansicpg1252\\deff0{\\fonttbl(\\f0\\fnil\\fcharset0 %s;)}{\\colortbl\\red0\\green0\\blue0;\\red255\\green0\\blue0;\\red0\\green0\\blue191;}\\deflang1033\\horzdoc\\fromtext\\pard\\plain\\f0\\fs%s" % (font, font_size))

	for line in fin:
		# kill line endings
		line = re.sub(r'[\r\n]', '', line)

		line = line + "\\par"

		# if a paragraph starts with " - " treat it as a bulleted paragraph
		if re.match(r'^\s-\s', line, re.A):
			line = re.sub(r'^\s-\s', '', line, flags=re.A)
			line = "\\pard{\\*\\pn\\pnlvlblt\\pnindent0{\\pntxtb\\'B7}}\\fi-144\\li144 " + line + "\\pard"

		# highlight URLs
		line = re.sub(r'(http://[\w~/.\-]*)', r'\\cf2 \1\\cf0', line, flags=re.A)

		# if a paragraph starts with "DISCLAIMER:" make the text bold and red
		if line.startswith("DISCLAIMER"):
			line = "\\cf1\\b " + line + "\\b0\\cf0"

		# if a paragraph starts with a quote make the text italic
		if line.startswith('"'):
			line = "\\i " + line + "\\i0"

		if escape:
			# Escape quotes and backslashes
			line = re.sub(r'(["\\])', r'\\\1', line)

		fout.write(line + " ")

	fout.write("}")
