#!/usr/bin/env python3

import sys

from render_func_common import open_outputs

fun, tab, tab_name = open_outputs(sys.argv)

# Generate optimized functions and a table with the function names

fun.write("#define EIGHTBIT 1\n")
fun.write("#define ROT 0\n\n")
fun.write("#define BPP 16\n")

tab.write("// Table with all function addresses.\n")
tab.write("static RenderSpriteFunction RenderSpriteFunctionTable[] = {\n")

zbuffer = ["_NOZBUFFER", "_RZBUFFER", "_WZBUFFER", "_RWZBUFFER"]

for size in (320, 384):
	fun.write("#define XSIZE %d\n" % size)

	function_name = "&RenderSprite16_%d_ROT0" % size

	# No scaling

	fun.write("#define ZOOM 0\n")

	for function in range(8):
		if (function & 3) == 0:
			fun.write("#define XFLIP %d\n" % ((function & 4) // 4))
			flip = "_NOFLIP" if (function & 4) == 0 else "_FLIPX"
			tab.write("\t")

		fun.write("#define ZBUFFER %d\n" % (function & 3))

		fun.write("#include \"cave_sprite_render.h\"\n")

		tab.write(function_name + flip + "_NOZOOM_CLIPX" + zbuffer[function & 3] + "_256, ")

		fun.write("#undef ZBUFFER\n")

		if (function & 3) == 3:
			fun.write("#undef XFLIP\n")
			tab.write("\n")

	fun.write("#undef ZOOM\n\n")

	# Scale up/down

	fun.write("#define XFLIP 0\n")

	for function in range(8):
		if (function & 3) == 0:
			fun.write("#define ZOOM %d\n" % (1 + (function & 4) // 4))
			zoom = "_ZOOMOUT" if (function & 4) == 0 else "_ZOOMIN"
			tab.write("\t")

		fun.write("#define ZBUFFER %d\n" % (function & 3))
		fun.write("#include \"cave_sprite_render_zoom.h\"\n")
		fun.write("#undef ZBUFFER\n")

		tab.write(function_name + "_NOFLIP" + zoom + "_NOCLIP" + zbuffer[function & 3] + "_256, ")

		if (function & 3) == 3:
			fun.write("#undef ZOOM\n\n")
			tab.write("\n")

	fun.write("#undef XFLIP\n")
	fun.write("#undef XSIZE\n\n")

fun.write("#undef BPP\n\n")
fun.write("#undef ROT\n\n")
fun.write("#undef EIGHTBIT\n\n")

tab.write("};\n\n")

fun.write("#include \"%s\"\n" % tab_name)

tab.write("static RenderSpriteFunction* RenderSprite_ROT0[2] = {\n\t&RenderSpriteFunctionTable[0],\n\t&RenderSpriteFunctionTable[16]\n};\n")

tab.close()
fun.close()
