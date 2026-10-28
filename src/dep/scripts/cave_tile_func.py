#!/usr/bin/env python3

import sys

from render_func_common import open_outputs

fun, tab, tab_name = open_outputs(sys.argv)

# Generate optimized functions and a table with the function names

fun.write("#define XFLIP 0\n")
fun.write("#define YFLIP 0\n")
fun.write("#define ROT 0\n\n")
fun.write("#define BPP 16\n")

tab.write("// Table with all function addresses.\n")
tab.write("static RenderTileFunction RenderTileFunctionTable[] = {\n")

for size in (320, 384):
	fun.write("#define XSIZE %d\n" % size)
	fun.write("#define EIGHTBIT 1\n")

	function_name = "&RenderTile16_%d_ROT0_NOFLIP" % size

	for function in range(3):
		if (function & 1) == 0:
			row_scroll = "_NOROWSCROLL"
			row_select = "_NOROWSELECT" if (function & 2) == 0 else "_ROWSELECT"
		else:
			row_scroll = "_ROWSCROLL"

		fun.write("#define ROWSCROLL %d\n" % (function & 1))
		fun.write("#define ROWSELECT %d\n" % ((function & 2) // 2))

		tab.write("\t")

		fun.write("#define DOCLIP 0\n")
		fun.write("#include \"cave_tile_render.h\"\n")
		fun.write("#undef DOCLIP\n")

		tab.write(function_name + row_scroll + row_select + "_NOCLIP_256, ")

		fun.write("#define DOCLIP 1\n")
		fun.write("#include \"cave_tile_render.h\"\n")
		fun.write("#undef DOCLIP\n")

		tab.write(function_name + row_scroll + row_select + "_CLIP_256, ")

		fun.write("#undef ROWSELECT\n")
		fun.write("#undef ROWSCROLL\n")

		tab.write("\n")
	fun.write("#undef EIGHTBIT\n\n")
	fun.write("#undef XSIZE\n\n")

fun.write("#undef BPP\n\n")
fun.write("#undef ROT\n\n")

fun.write("#undef YFLIP\n")
fun.write("#undef XFLIP\n\n")

tab.write("};\n\n")

fun.write("#include \"%s\"\n" % tab_name)

tab.write("static RenderTileFunction* RenderTile_ROT0[2] = {\n\t&RenderTileFunctionTable[0],\n\t&RenderTileFunctionTable[6]\n};\n")

tab.close()
fun.close()
