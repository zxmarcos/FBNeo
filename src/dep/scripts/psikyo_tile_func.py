#!/usr/bin/env python3

import sys

from render_func_common import open_outputs

fun, tab, tab_name = open_outputs(sys.argv)

# Generate optimized functions and a table with the function names

trans_fun = [0, 15, -1]
trans_tab = ["_TRANS0", "_TRANS15", "_SOLID"]

fun.write("#define ROT 0\n")
fun.write("#define BPP 16\n")
fun.write("#define FLIP 0\n")
fun.write("#define ZOOM 0\n")
fun.write("#define ZBUFFER 0\n")
fun.write("\n")

tab.write("// Table with all function addresses.\n")
tab.write("static RenderTileFunction RenderTile[] = {\n")

for function in range(6):
	function_name = "&RenderTile16" + trans_tab[(function & 6) // 2] + "_NOFLIP_ROT0"

	if (function & 1) == 0:
		row_scroll = "_NOROWSCROLL"
		fun.write("#define TRANS %d\n\n" % trans_fun[(function & 6) // 2])
	else:
		row_scroll = "_ROWSCROLL"

	fun.write("#define ROWSCROLL %d\n" % (function & 1))
	tab.write("\t")

	fun.write("#define DOCLIP 0\n")
	fun.write("#include \"psikyo_render.h\"\n")
	fun.write("#undef DOCLIP\n")

	tab.write(function_name + row_scroll + "_NOZOOM_NOZBUFFER_NOCLIP, ")

	fun.write("#define DOCLIP 1\n")
	fun.write("#include \"psikyo_render.h\"\n")
	fun.write("#undef DOCLIP\n")

	tab.write(function_name + row_scroll + "_NOZOOM_NOZBUFFER_CLIP, ")

	fun.write("#undef ROWSCROLL\n")
	if (function & 1) == 1:
		fun.write("\n#undef TRANS\n")

	tab.write("\n")

fun.write("#undef ZBUFFER\n")
fun.write("#undef ZOOM\n")
fun.write("#undef FLIP\n")
fun.write("#undef BPP\n")
fun.write("#undef ROT\n")
fun.write("\n")

tab.write("};\n\n")

fun.write("#include \"%s\"\n" % tab_name)

tab.close()
fun.close()
