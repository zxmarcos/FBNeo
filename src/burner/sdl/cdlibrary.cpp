// The CD library of win32/cdlistcache.cpp (the scan of the Neo Geo CD and PC
// Engine CD images, with a cache), for the ImGui interface: that file has the
// other systems too
#include "burner.h"
#include "cd_img.h"						// CDIMAGE_MAX_SOURCE_FILES, from burner_win32.h in the Win32 build

// the texts of the cache are UTF-8, as the ones of the SDL2 frontend
static char* utf8_from_tstring(const TCHAR* pszText)
{
	return strdup(pszText);
}

static TCHAR* tstring_from_utf8(const char* pszText)
{
	return strdup(pszText);
}

#include "../win32/cdlistcache.cpp"
