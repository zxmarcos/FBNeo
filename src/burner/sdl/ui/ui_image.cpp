// Images for the interface (previews, titles, icons) as renderer textures,
// from a file or from an entry of a zip
#include "burner.h"
#include "ui.h"
#include "zip.h"
#include <SDL_image.h>

extern SDL_Renderer* sdlRenderer;			// vid_sdl2.cpp

static SDL_Texture* TextureFromRW(SDL_RWops* rw, int* pnWidth, int* pnHeight)
{
	if (rw == NULL) {
		return NULL;
	}

	SDL_Surface* surface = IMG_Load_RW(rw, 1);
	if (surface == NULL) {
		return NULL;
	}

	SDL_Texture* texture = SDL_CreateTextureFromSurface(sdlRenderer, surface);
	if (texture) {
		SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
		if (pnWidth) *pnWidth = surface->w;
		if (pnHeight) *pnHeight = surface->h;
	}
	SDL_FreeSurface(surface);

	return texture;
}

SDL_Texture* UiImageLoad(const char* pszFile, int* pnWidth, int* pnHeight)
{
	FILE* fp = fopen(pszFile, "rb");				// cheaper than SDL_RWFromFile() for the files that don't exist
	if (fp == NULL) {
		return NULL;
	}
	fclose(fp);

	return TextureFromRW(SDL_RWFromFile(pszFile, "rb"), pnWidth, pnHeight);
}

UiZip* UiZipOpen(const char* pszFile)
{
	return (UiZip*)zip_open(pszFile, 0, 'r');
}

void UiZipClose(UiZip* pZip)
{
	if (pZip) {
		zip_close((zip_t*)pZip);
	}
}

SDL_Texture* UiImageLoadZip(UiZip* pZip, const char* pszEntry, int* pnWidth, int* pnHeight)
{
	if (pZip == NULL) {
		return NULL;
	}

	zip_t* zip = (zip_t*)pZip;
	void* pBuffer = NULL;
	size_t nSize = 0;
	SDL_Texture* texture = NULL;

	if (zip_entry_open(zip, pszEntry) == 0) {
		if (zip_entry_read(zip, &pBuffer, &nSize) >= 0 && pBuffer) {
			texture = TextureFromRW(SDL_RWFromConstMem(pBuffer, (int)nSize), pnWidth, pnHeight);
		}
		zip_entry_close(zip);
	}
	free(pBuffer);

	return texture;
}

SDL_Texture* UiImageLoadFromZipFile(const char* pszZip, const char* pszEntry, int* pnWidth, int* pnHeight)
{
	UiZip* pZip = UiZipOpen(pszZip);
	SDL_Texture* texture = UiImageLoadZip(pZip, pszEntry, pnWidth, pnHeight);
	UiZipClose(pZip);

	return texture;
}
