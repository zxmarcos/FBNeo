#!/usr/bin/env python3

import sys

from render_func_common import open_outputs

fun, tab, tab_name = open_outputs(sys.argv)

# Generate optimized functions and a table with the function names

tab.write("// Table with all function addresses.\n")
tab.write("static RenderTileFunction RenderTileFunctionTable[] = {\n")

flip = ["_NOFLIP", "_FLIPX", "_FLIPY", "_FLIPXY"]

for rot in (0, 270):
	if rot == 270:
		fun.write("#ifdef DRIVER_ROTATION\n")
		tab.write("#ifdef DRIVER_ROTATION\n")

	fun.write("#define ROT %d\n\n" % rot)
	for bitdepth in (16, 24, 32):
		fun.write("// %d-bit rendering functions.\n" % bitdepth)
		fun.write("#define BPP %d\n\n" % bitdepth)

		function_name = "&RenderTile%d_ROT%d" % (bitdepth, rot)

		for function in range(8):
			if (function & 3) == 0:
				fun.write("#define ISOPAQUE %d\n" % ((function & 4) // 4))
				tab.write("\t")
				is_opaque = "_TRANS" if (function & 4) == 0 else "_OPAQUE"

			fun.write("#define XFLIP %d\n" % (function & 1))
			fun.write("#define YFLIP %d\n" % ((function & 2) // 2))

			fun.write("#define DOCLIP 0\n")
			fun.write("#include \"toa_gp9001_render.h\"\n")
			fun.write("#undef DOCLIP\n")

			tab.write(function_name + flip[function & 3] + "_NOCLIP" + is_opaque + ", ")

			fun.write("#define DOCLIP 1\n")
			fun.write("#include \"toa_gp9001_render.h\"\n")
			fun.write("#undef DOCLIP\n")

			tab.write(function_name + flip[function & 3] + "_CLIP" + is_opaque + ", ")

			fun.write("#undef YFLIP\n")
			fun.write("#undef XFLIP\n\n")

			if (function & 3) == 3:
				fun.write("#undef ISOPAQUE\n\n")
				tab.write("\n")
		fun.write("#undef BPP\n\n")
	fun.write("#undef ROT\n\n")

	if rot == 270:
		fun.write("#endif\n")
		tab.write("#endif\n")

fun.write("\n")

tab.write("};\n\n")

fun.write("#include \"%s\"\n" % tab_name)

tab.write("static RenderTileFunction* RenderTile_ROT0[3] = {\n\t&RenderTileFunctionTable[0],\n\t&RenderTileFunctionTable[16],\n\t&RenderTileFunctionTable[32]\n};\n")
tab.write("#ifdef DRIVER_ROTATION\n")
tab.write("static RenderTileFunction* RenderTile_ROT270[3] = {\n\t&RenderTileFunctionTable[48],\n\t&RenderTileFunctionTable[64],\n\t&RenderTileFunctionTable[80]\n};\n")
tab.write("#endif\n")

tab.close()
fun.close()
