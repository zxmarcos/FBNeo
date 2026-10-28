#!/usr/bin/env python3

import sys

from render_func_common import open_outputs

fun, tab, tab_name = open_outputs(sys.argv)

# Generate optimized functions and a table with the function names

tab.write("// Table with all function addresses.\n")
tab.write("static RenderBankFunction RenderBankFunctionTable[] = {\n")

fun.write("#define ISOPAQUE 0\n\n")

for bitdepth in (16, 24, 32):
	fun.write("// %d-bit rendering functions.\n" % bitdepth)
	fun.write("#define BPP %d\n\n" % bitdepth)

	function_name = "&RenderBank%d" % bitdepth

	for function in range(32):
		if (function & 15) == 0:
			fun.write("#define DOCLIP %d\n" % ((function & 16) // 16))
			do_clip = "_NOCLIP" if (function & 16) == 0 else "_CLIP"
			tab.write("\t")

		fun.write("#define XZOOM %d\n" % (function & 15))
		fun.write("#include \"neo_sprite_render.h\"\n")
		fun.write("#undef XZOOM\n")

		tab.write("%s_ZOOM%d%s_TRANS, " % (function_name, function & 15, do_clip))

		if (function & 15) == 15:
			fun.write("#undef DOCLIP\n")
			tab.write("\n")

	fun.write("#undef BPP\n\n")
fun.write("#undef ISOPAQUE\n\n")

fun.write("#include \"%s\"\n" % tab_name)

tab.write("};\n\n")
tab.write("static RenderBankFunction* RenderBankNormal[3] = {\n\t&RenderBankFunctionTable[0],\n\t&RenderBankFunctionTable[32],\n\t&RenderBankFunctionTable[64]\n};\n")

tab.close()
fun.close()
