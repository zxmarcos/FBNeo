// CD library (win32/neocdsel.cpp): the Neo Geo CD and PC Engine CD images of
// a folder, scanned in a thread by win32/cdlistcache.cpp (which keeps a
// cache), with their covers and previews, and Play
#include "burner.h"
#include "../../win32/cdlistcache.h"
#include "ui.h"
#include "imgui.h"
#include <vector>
#include <algorithm>

char szUiCdLibraryDir[MAX_PATH] = "cdiso/";
bool bUiCdLibrarySubdirs = true;

static bool bRequest = false;
static char szEditDir[MAX_PATH];

// the scan, in a thread
static SDL_Thread* pScanThread = NULL;
static SDL_atomic_t nScanCancel;
static SDL_atomic_t nScanDone;
static SDL_atomic_t nScanTotal;
static SDL_atomic_t nScanCount;
static SDL_atomic_t nScanPhase;
static CDLibrary* pScanResult = NULL;
static INT32 nScanStatus = CDLIBRARY_SCAN_OK;

static CDLibrary* pLibrary = NULL;
static std::vector<int> Order;				// the rows, sorted
static int nSortColumn = 0;
static bool bSortDescending = false;
static int nSelected = -1;					// in pLibrary->pEntries

struct Image {
	SDL_Texture* pTexture;
	int nWidth, nHeight;
};
static Image Images[2] = { { NULL, 0, 0 }, { NULL, 0, 0 } };	// cover, preview
static int nImagesFor = -1;
static int nZoom = -1;						// the image shown big

static void FreeImages()
{
	for (int i = 0; i < 2; i++) {
		if (Images[i].pTexture) {
			SDL_DestroyTexture(Images[i].pTexture);
		}
		Images[i].pTexture = NULL;
		Images[i].nWidth = Images[i].nHeight = 0;
	}
	nImagesFor = -1;
}

static const char* PlatformText(INT32 nPlatform)
{
	if (nPlatform == CDLIST_PLATFORM_NEOCD) return "Neo Geo CD";
	if (nPlatform == CDLIST_PLATFORM_PCECD) return "PC Engine CD";
	return "Unknown";
}

// ----------------------------------------------------------------------------
// Scan

static INT32 ScanCancelled(void*)
{
	return SDL_AtomicGet(&nScanCancel);
}

static void ScanProgress(INT32 nPhase, UINT32 nTotal, UINT32 nCompleted, void*)
{
	SDL_AtomicSet(&nScanPhase, nPhase);
	SDL_AtomicSet(&nScanTotal, (int)nTotal);
	SDL_AtomicSet(&nScanCount, (int)nCompleted);
}

static int ScanProc(void*)
{
	nScanStatus = CDLibraryScan(szUiCdLibraryDir, bUiCdLibrarySubdirs ? 1 : 0, ScanCancelled, ScanProgress, NULL, &pScanResult);
	SDL_AtomicSet(&nScanDone, 1);
	return 0;
}

static bool Scanning()
{
	return pScanThread != NULL;
}

static void SortRows();

static void ScanStart()
{
	if (Scanning()) {
		return;
	}
	SDL_AtomicSet(&nScanCancel, 0);
	SDL_AtomicSet(&nScanDone, 0);
	SDL_AtomicSet(&nScanTotal, 0);
	SDL_AtomicSet(&nScanCount, 0);
	SDL_AtomicSet(&nScanPhase, CDLIBRARY_PHASE_ENUMERATE);
	pScanResult = NULL;
	pScanThread = SDL_CreateThread(ScanProc, "cd library scan", NULL);
	if (pScanThread == NULL) {
		ScanProc(NULL);
	}
}

static void ScanEnd()
{
	if (pScanThread) {
		SDL_WaitThread(pScanThread, NULL);
		pScanThread = NULL;
	}
	if (pScanResult) {
		CDLibraryFree(pLibrary);
		pLibrary = pScanResult;
		pScanResult = NULL;
		nSelected = -1;
		FreeImages();
		SortRows();
	} else if (nScanStatus == CDLIBRARY_SCAN_INVALID_ARGUMENT) {
		CDLibraryFree(pLibrary);			// no such folder
		pLibrary = NULL;
		Order.clear();
		nSelected = -1;
		FreeImages();
	}
}

static void ScanStop()
{
	if (Scanning()) {
		SDL_AtomicSet(&nScanCancel, 1);
		ScanEnd();
	}
}

// ----------------------------------------------------------------------------
// The list

static int CompareText(const char* a, const char* b)
{
	return _stricmp(a, b);
}

static bool RowLess(int a, int b)
{
	const CDLibraryEntry* pA = &pLibrary->pEntries[a];
	const CDLibraryEntry* pB = &pLibrary->pEntries[b];
	int nResult = 0;
	switch (nSortColumn) {
		case 1: nResult = CompareText(PlatformText(pA->nPlatform), PlatformText(pB->nPlatform)); break;
		case 2:
			if (pA->nPlatform != pB->nPlatform) nResult = pA->nPlatform - pB->nPlatform;
			else if (pA->nPlatform == CDLIST_PLATFORM_NEOCD) nResult = (pA->nNeoID < pB->nNeoID) ? -1 : (pA->nNeoID > pB->nNeoID);
			else nResult = CompareText(pA->szShortName, pB->szShortName);
			break;
		case 3: nResult = pA->nAudioTrackCount - pB->nAudioTrackCount; break;
		default: nResult = CompareText(pA->szTitle, pB->szTitle); break;
	}
	if (nResult == 0) {
		nResult = CompareText(pA->szPath, pB->szPath);
	}
	return bSortDescending ? nResult > 0 : nResult < 0;
}

static void SortRows()
{
	Order.clear();
	if (pLibrary == NULL) {
		return;
	}
	for (UINT32 i = 0; i < pLibrary->nCount; i++) {
		Order.push_back((int)i);
	}
	std::sort(Order.begin(), Order.end(), RowLess);
}

// ngcd_<name>.png / pcecd_<name>.png in the folder, or in its <archive>.zip
static void LoadImage(Image* pImage, const char* pszDir, const char* pszArchive, const char* pszName)
{
	char szFile[MAX_PATH * 2];
	snprintf(szFile, sizeof(szFile), "%s%s.png", pszDir, pszName);
	pImage->pTexture = UiImageLoad(szFile, &pImage->nWidth, &pImage->nHeight);
	if (pImage->pTexture == NULL) {
		char szZip[MAX_PATH * 2];
		snprintf(szZip, sizeof(szZip), "%s%s.zip", pszDir, pszArchive);
		snprintf(szFile, sizeof(szFile), "%s/%s.png", pszArchive, pszName);
		pImage->pTexture = UiImageLoadFromZipFile(szZip, szFile, &pImage->nWidth, &pImage->nHeight);
	}
}

static void LoadImages(int nEntry)
{
	FreeImages();
	nImagesFor = nEntry;
	if (nEntry < 0) {
		return;
	}
	const CDLibraryEntry* e = &pLibrary->pEntries[nEntry];
	char szName[MAX_PATH];
	snprintf(szName, sizeof(szName), "%s%s", e->nPlatform == CDLIST_PLATFORM_PCECD ? "pcecd_" : "ngcd_", e->szShortName[0] ? e->szShortName : e->szTitle);

	LoadImage(&Images[0], szAppCdCoversPath, "cdcovers", szName);
	LoadImage(&Images[1], szAppCdPreviewsPath, "cdpreviews", szName);
	if (Images[1].pTexture == NULL) {
		LoadImage(&Images[1], szAppPreviewsPath, "previews", szName);
	}
}

static void Play(int nEntry)
{
	const CDLibraryEntry* e = &pLibrary->pEntries[nEntry];
	ScanStop();
	UiGameQuickLoadCD(e->szPath);			// neocdz or pce_scdsys, told by the image
	ImGui::CloseCurrentPopup();
}

// ----------------------------------------------------------------------------

void UiCdLibraryOpen()
{
	bRequest = true;
}

static void DrawImage(int i, float fWidth, float fHeight)
{
	static const char* pszNames[2] = { "Cover", "Preview" };
	ImGui::TextDisabled("%s", pszNames[i]);
	ImVec2 box(fWidth, fHeight);
	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(pos, ImVec2(pos.x + box.x, pos.y + box.y), IM_COL32(0, 0, 0, 120));
	const Image& img = Images[i];
	if (img.pTexture && img.nWidth > 0 && img.nHeight > 0) {
		float fScale = box.x / img.nWidth;
		if (img.nHeight * fScale > box.y) fScale = box.y / img.nHeight;
		ImVec2 size(img.nWidth * fScale, img.nHeight * fScale);
		ImVec2 at(pos.x + (box.x - size.x) * 0.5f, pos.y + (box.y - size.y) * 0.5f);
		dl->AddImage((ImTextureID)(intptr_t)img.pTexture, at, ImVec2(at.x + size.x, at.y + size.y));
	} else {
		ImVec2 size = ImGui::CalcTextSize("No image");
		dl->AddText(ImVec2(pos.x + (box.x - size.x) * 0.5f, pos.y + (box.y - size.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), "No image");
	}
	ImGui::PushID(i);
	if (ImGui::InvisibleButton("##image", box) && img.pTexture) {
		nZoom = i;							// a click shows it big
	}
	ImGui::PopID();
}

static void DrawZoom()
{
	if (nZoom >= 0 && !ImGui::IsPopupOpen("###cdzoom")) {
		ImGui::OpenPopup("###cdzoom");
	}
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x * 0.85f, vp->WorkSize.y * 0.85f), ImGuiCond_Appearing);
	if (!ImGui::BeginPopupModal(nZoom == 0 ? "Cover###cdzoom" : "Preview###cdzoom", NULL, ImGuiWindowFlags_NoSavedSettings)) {
		return;
	}
	const Image& img = Images[nZoom >= 0 ? nZoom : 0];
	ImVec2 box = ImGui::GetContentRegionAvail();
	box.y -= ImGui::GetFrameHeightWithSpacing();
	if (img.pTexture && img.nWidth > 0 && img.nHeight > 0) {
		float fScale = box.x / img.nWidth;
		if (img.nHeight * fScale > box.y) fScale = box.y / img.nHeight;
		ImVec2 size(img.nWidth * fScale, img.nHeight * fScale);
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (box.x - size.x) * 0.5f);
		ImGui::Image((ImTextureID)(intptr_t)img.pTexture, size);
	}
	if (ImGui::Button("Close") || UiEscapePressed() || img.pTexture == NULL) {
		nZoom = -1;
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

static void DrawToolbar()
{
	float fButtons = ImGui::CalcTextSize("...").x + ImGui::CalcTextSize("Subfolders").x + ImGui::CalcTextSize("Rescan").x + ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 6.0f + ImGui::GetStyle().ItemSpacing.x * 6.0f;
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Folder");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(-fButtons);
	if (ImGui::InputText("##dir", szEditDir, sizeof(szEditDir), ImGuiInputTextFlags_EnterReturnsTrue)) {
		snprintf(szUiCdLibraryDir, sizeof(szUiCdLibraryDir), "%s", szEditDir);
		ScanStop();
		ScanStart();
	}
	ImGui::SameLine();
	if (ImGui::Button("...")) {
		UiFileBrowserOpen("Choose a folder###cdbrowse", szEditDir, UI_BROWSE_FOLDER, NULL);
	}
	char szPicked[MAX_PATH];
	if (UiFileBrowserDraw("Choose a folder###cdbrowse", szPicked, sizeof(szPicked))) {
		snprintf(szEditDir, sizeof(szEditDir), "%s", szPicked);
		snprintf(szUiCdLibraryDir, sizeof(szUiCdLibraryDir), "%s", szPicked);
		ScanStop();
		ScanStart();
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Subfolders", &bUiCdLibrarySubdirs)) {
		ScanStop();
		ScanStart();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(Scanning());
	if (ImGui::Button("Rescan")) {
		ScanStart();
	}
	ImGui::EndDisabled();

	if (Scanning()) {
		int nTotal = SDL_AtomicGet(&nScanTotal), nCount = SDL_AtomicGet(&nScanCount);
		char szText[96];
		if (SDL_AtomicGet(&nScanPhase) == CDLIBRARY_PHASE_ENUMERATE) {
			snprintf(szText, sizeof(szText), "Looking for the images... %d found", nCount);
		} else {
			snprintf(szText, sizeof(szText), "Identifying %d of %d", nCount, nTotal);
		}
		float fCancel = ImGui::CalcTextSize("Cancel").x + ImGui::GetStyle().FramePadding.x * 2.0f;
		ImGui::ProgressBar(nTotal ? (float)nCount / nTotal : 0.0f, ImVec2(-(fCancel + ImGui::GetStyle().ItemSpacing.x), 0.0f), szText);
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			ScanStop();
		}
	}
}

static void DrawList(float fHeight)
{
	ImGuiTableFlags nFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable;
	if (!ImGui::BeginTable("##cds", 4, nFlags, ImVec2(0.0f, fHeight))) {
		return;
	}
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort, 4.0f);
	ImGui::TableSetupColumn("Platform", ImGuiTableColumnFlags_WidthStretch, 1.4f);
	ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthStretch, 1.0f);
	ImGui::TableSetupColumn("Tracks", ImGuiTableColumnFlags_WidthStretch, 0.8f);
	ImGui::TableHeadersRow();

	if (ImGuiTableSortSpecs* pSpecs = ImGui::TableGetSortSpecs()) {
		if (pSpecs->SpecsDirty && pSpecs->SpecsCount > 0) {
			nSortColumn = pSpecs->Specs[0].ColumnIndex;
			bSortDescending = pSpecs->Specs[0].SortDirection == ImGuiSortDirection_Descending;
			SortRows();
			pSpecs->SpecsDirty = false;
		}
	}

	ImGuiListClipper clipper;
	clipper.Begin((int)Order.size());
	while (clipper.Step()) {
		for (int r = clipper.DisplayStart; r < clipper.DisplayEnd; r++) {
			int i = Order[r];
			const CDLibraryEntry* e = &pLibrary->pEntries[i];
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::PushID(i);
			if (ImGui::Selectable(UiText(e->szTitle), nSelected == i, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
				nSelected = i;
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					Play(i);
				}
			}
			ImGui::SetItemTooltip("%s", UiText(e->szPath));
			ImGui::PopID();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(PlatformText(e->nPlatform));
			ImGui::TableNextColumn();
			if (e->nPlatform == CDLIST_PLATFORM_NEOCD && e->nNeoID) {
				ImGui::Text("%04X", e->nNeoID);
			} else {
				ImGui::TextUnformatted(UiText(e->szShortName));
			}
			ImGui::TableNextColumn();
			ImGui::Text("%d", e->nAudioTrackCount);
		}
	}
	ImGui::EndTable();
}

static void DrawDetails()
{
	if (nSelected < 0 || pLibrary == NULL) {
		ImGui::TextDisabled("Select a CD to see its details.");
		return;
	}
	const CDLibraryEntry* e = &pLibrary->pEntries[nSelected];
	if (nImagesFor != nSelected) {
		LoadImages(nSelected);
	}

	if (ImGui::BeginTable("##details", 2, ImGuiTableFlags_SizingFixedFit)) {
		ImGui::TableSetupColumn("##label");
		ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);
		const char* pszLabels[5] = { "Title:", "Short name:", "Publisher:", "Image:", "Audio tracks:" };
		for (int i = 0; i < 5; i++) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextDisabled("%s", pszLabels[i]);
			ImGui::TableNextColumn();
			switch (i) {
				case 0: ImGui::TextWrapped("%s", UiText(e->szTitle)); break;
				case 1: ImGui::TextUnformatted(UiText(e->szShortName)); break;
				case 2:
					if (e->szCompany[0] && e->szYear[0]) ImGui::Text("%s (%s)", UiText(e->szCompany), e->szYear);
					else ImGui::Text("%s%s", UiText(e->szCompany), e->szYear);
					break;
				case 3: ImGui::TextWrapped("%s", UiText(e->szDataPath[0] ? e->szDataPath : e->szPath)); break;
				case 4: ImGui::Text("%d", e->nAudioTrackCount); break;
			}
		}
		ImGui::EndTable();
	}

	float fWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
	float fHeight = ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing() - ImGui::GetStyle().ItemSpacing.y;
	if (fHeight > fWidth * 1.2f) fHeight = fWidth * 1.2f;
	ImGui::BeginGroup();
	DrawImage(0, fWidth, fHeight);
	ImGui::EndGroup();
	ImGui::SameLine();
	ImGui::BeginGroup();
	DrawImage(1, fWidth, fHeight);
	ImGui::EndGroup();
}

void UiCdLibraryDraw()
{
	if (bRequest) {
		bRequest = false;
		snprintf(szEditDir, sizeof(szEditDir), "%s", szUiCdLibraryDir);
		nZoom = -1;
		ScanStart();						// quick with the cache
		ImGui::OpenPopup("CD Library###cdlibrary");
	}

	if (Scanning() && SDL_AtomicGet(&nScanDone)) {
		ScanEnd();
	}

	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(UiDialogSize(960.0f, vp->WorkSize.y * 0.85f), ImGuiCond_Appearing);
	if (!ImGui::BeginPopupModal("CD Library###cdlibrary", NULL, ImGuiWindowFlags_NoSavedSettings)) {
		if (pLibrary && !ImGui::IsPopupOpen("CD Library###cdlibrary") && !Scanning()) {
			FreeImages();					// closed: the list is kept for the next time
		}
		return;
	}

	DrawToolbar();
	ImGui::Spacing();

	float fBottom = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
	float fMain = ImGui::GetContentRegionAvail().y - fBottom;
	if (ImGui::BeginTable("##layout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV, ImVec2(0.0f, fMain))) {
		ImGui::TableSetupColumn("##list", ImGuiTableColumnFlags_WidthStretch, 1.6f);
		ImGui::TableSetupColumn("##info", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if (pLibrary && pLibrary->nCount) {
			DrawList(fMain - ImGui::GetStyle().CellPadding.y * 2.0f);
		} else if (!Scanning()) {
			ImGui::TextWrapped("No Neo Geo CD or PC Engine CD image (.cue, .ccd, .chd) in %s.", UiText(szUiCdLibraryDir));
		}
		ImGui::TableNextColumn();
		ImGui::BeginChild("##details", ImVec2(0.0f, fMain - ImGui::GetStyle().CellPadding.y * 2.0f));
		DrawDetails();
		ImGui::EndChild();
		ImGui::EndTable();
	}

	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("%u CDs", pLibrary ? pLibrary->nCount : 0);
	float fButton = 120.0f * fUiScale;
	ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - fButton * 2.0f - ImGui::GetStyle().ItemSpacing.x);
	ImGui::BeginDisabled(nSelected < 0);
	if (ImGui::Button("Play", ImVec2(fButton, 0.0f))) {
		Play(nSelected);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Close", ImVec2(fButton, 0.0f)) || (nZoom < 0 && UiEscapePressed())) {
		ScanStop();
		ImGui::CloseCurrentPopup();
	}

	DrawZoom();
	ImGui::EndPopup();
}

void UiCdLibraryExit()
{
	ScanStop();
	FreeImages();
	CDLibraryFree(pLibrary);
	pLibrary = NULL;
	Order.clear();
}
