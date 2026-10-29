// RomData manager (win32/romdata.cpp): the dats of szAppRomdataPath, each one a
// romset for a driver (a hack), with its title and preview, and Play; and the
// export of a template dat from a driver of the selector
#include "burner.h"
#include "gamelist_model.h"
#include "ui.h"
#include "imgui.h"
#include <dirent.h>
#include <sys/stat.h>
#include <vector>
#include <algorithm>

bool bUiRomDataSubdirs = false;

static bool bRequest = false;
static std::vector<RomDataListInfo> Dats;
static std::vector<int> Order;
static int nSortColumn = 0;
static bool bSortDescending = false;
static int nSelected = -1;					// in Dats
static char szPendingDat[MAX_PATH] = "";	// set as szRomdataName by StartGame()

static int nExportDrv = -1;
static bool bExportRequest = false;

struct Image {
	SDL_Texture* pTexture;
	int nWidth, nHeight;
};
static Image Images[2] = { { NULL, 0, 0 }, { NULL, 0, 0 } };	// title, preview
static int nImagesFor = -1;

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

// ----------------------------------------------------------------------------
// The launch

bool UiRomDataPlay(const char* pszDat)
{
	INT32 nDrv = RomDataCheck(pszDat);
	if (nDrv < 0) {
		return false;
	}
	snprintf(szPendingDat, sizeof(szPendingDat), "%s", pszDat);
	nBurnDrvActive = nDrv;
	UiLaunchGame(nDrv);
	return true;
}

// From StartGame(), after the previous game is over
void UiRomDataPrepareLaunch()
{
	_tcscpy(szRomdataName, szPendingDat);
	szPendingDat[0] = '\0';
}

// ----------------------------------------------------------------------------
// The list

static bool IsDat(const char* pszName)
{
	int nLen = strlen(pszName);
	return nLen > 4 && _stricmp(pszName + nLen - 4, ".dat") == 0;
}

static void FindDats(const char* pszDir, int nDepth)
{
	DIR* d = opendir(pszDir);
	if (d == NULL) {
		return;
	}
	struct dirent* ent;
	while ((ent = readdir(d)) != NULL) {
		if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
			continue;
		}
		char szPath[MAX_PATH * 2];
		int nLen = strlen(pszDir);
		bool bSlash = nLen && (pszDir[nLen - 1] == '/' || pszDir[nLen - 1] == '\\');
		snprintf(szPath, sizeof(szPath), "%s%s%s", pszDir, bSlash ? "" : "/", ent->d_name);

		struct stat st;
		if (stat(szPath, &st) != 0) {
			continue;
		}
		if ((st.st_mode & S_IFMT) == S_IFDIR) {
			if (bUiRomDataSubdirs && nDepth < 8) {
				FindDats(szPath, nDepth + 1);
			}
		} else if (IsDat(ent->d_name)) {
			RomDataListInfo Info;
			if (RomDataGetListInfo(szPath, &Info) == 0) {
				Dats.push_back(Info);
			}
		}
	}
	closedir(d);
}

static bool RowLess(int a, int b)
{
	const RomDataListInfo& A = Dats[a];
	const RomDataListInfo& B = Dats[b];
	int nResult = 0;
	switch (nSortColumn) {
		case 1: nResult = _stricmp(A.szZipName, B.szZipName); break;
		case 2: nResult = _stricmp(A.szDrvName, B.szDrvName); break;
		case 3: nResult = _stricmp(A.szHardware, B.szHardware); break;
		default: nResult = _stricmp(A.szFullName, B.szFullName); break;
	}
	if (nResult == 0) {
		nResult = _stricmp(A.szDat, B.szDat);
	}
	return bSortDescending ? nResult > 0 : nResult < 0;
}

static void SortRows()
{
	Order.clear();
	for (size_t i = 0; i < Dats.size(); i++) {
		Order.push_back((int)i);
	}
	std::sort(Order.begin(), Order.end(), RowLess);
}

static void Scan()
{
	UiGameSelPauseScan();					// RomDataGetListInfo() uses nBurnDrvActive
	Dats.clear();
	nSelected = -1;
	FreeImages();
	FindDats(szAppRomdataPath, 0);
	SortRows();
}

// <path><zip>.png, or the driver's
static void LoadImage(Image* pImage, const char* pszPath, const RomDataListInfo& Info)
{
	const char* pszNames[2] = { Info.szZipName, Info.szDrvName };
	for (int i = 0; i < 2 && pImage->pTexture == NULL; i++) {
		char szFile[MAX_PATH * 2];
		snprintf(szFile, sizeof(szFile), "%s%s.png", pszPath, pszNames[i]);
		pImage->pTexture = UiImageLoad(szFile, &pImage->nWidth, &pImage->nHeight);
	}
}

static void LoadImages(int nDat)
{
	FreeImages();
	nImagesFor = nDat;
	if (nDat >= 0) {
		LoadImage(&Images[0], szAppTitlesPath, Dats[nDat]);
		LoadImage(&Images[1], szAppPreviewsPath, Dats[nDat]);
	}
}

// ----------------------------------------------------------------------------

void UiRomDataOpen()
{
	bRequest = true;
}

void UiRomDataExportOpen(int nDrv)
{
	if (nDrv >= 0 && (UINT32)nDrv < nBurnDrvCount) {
		nExportDrv = nDrv;
		bExportRequest = true;
	}
}

static void DrawImage(int i, float fWidth, float fHeight)
{
	static const char* pszNames[2] = { "Title", "Preview" };
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
	ImGui::Dummy(box);
}

static void DrawList(float fHeight)
{
	ImGuiTableFlags nFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable;
	if (!ImGui::BeginTable("##dats", 4, nFlags, ImVec2(0.0f, fHeight))) {
		return;
	}
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort, 4.0f);
	ImGui::TableSetupColumn("Romset", ImGuiTableColumnFlags_WidthStretch, 1.2f);
	ImGui::TableSetupColumn("Driver", ImGuiTableColumnFlags_WidthStretch, 1.2f);
	ImGui::TableSetupColumn("Hardware", ImGuiTableColumnFlags_WidthStretch, 2.0f);
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
			const RomDataListInfo& Info = Dats[i];
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::PushID(i);
			if (ImGui::Selectable(UiText(Info.szFullName), nSelected == i, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
				nSelected = i;
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && UiRomDataPlay(Info.szDat)) {
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SetItemTooltip("%s", UiText(Info.szDat));
			ImGui::PopID();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(Info.szZipName);
			ImGui::TableNextColumn();
			if (Info.nDrv < 0) {
				ImGui::TextDisabled("%s (missing)", Info.szDrvName);
			} else {
				ImGui::TextUnformatted(Info.szDrvName);
			}
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(UiText(Info.szHardware));
		}
	}
	ImGui::EndTable();
}

static void DrawDetails()
{
	if (nSelected < 0) {
		ImGui::TextDisabled("Select a dat to see its details.");
		return;
	}
	const RomDataListInfo& Info = Dats[nSelected];
	if (nImagesFor != nSelected) {
		LoadImages(nSelected);
	}

	if (ImGui::BeginTable("##details", 2, ImGuiTableFlags_SizingFixedFit)) {
		ImGui::TableSetupColumn("##label");
		ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);
		const char* pszLabels[4] = { "Name:", "Romset:", "Driver:", "Dat:" };
		for (int i = 0; i < 4; i++) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextDisabled("%s", pszLabels[i]);
			ImGui::TableNextColumn();
			switch (i) {
				case 0: ImGui::TextWrapped("%s", UiText(Info.szFullName)); break;
				case 1: ImGui::TextUnformatted(Info.szZipName); break;
				case 2:
					if (Info.nDrv >= 0) ImGui::TextWrapped("%s (%s)", Info.szDrvName, UiText(GameListGet(Info.nDrv)->pszFullName));
					else ImGui::TextWrapped("%s, not in this version", Info.szDrvName);
					break;
				case 3: ImGui::TextWrapped("%s", UiText(Info.szDat)); break;
			}
		}
		ImGui::EndTable();
	}

	float fWidth = ImGui::GetContentRegionAvail().x;
	float fHeight = (ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing() * 2.0f - ImGui::GetStyle().ItemSpacing.y * 2.0f) * 0.5f;
	if (fHeight > fWidth * 0.75f) fHeight = fWidth * 0.75f;
	DrawImage(0, fWidth, fHeight);
	DrawImage(1, fWidth, fHeight);
}

static void DrawExport()
{
	if (bExportRequest) {
		bExportRequest = false;
		char szFile[MAX_PATH * 2];
		MakeDirs(szAppRomdataPath);
		snprintf(szFile, sizeof(szFile), "%stemplate_%s.dat", szAppRomdataPath, GameListGet(nExportDrv)->pszName);
		UiFileBrowserOpen("Export RomData Template###rdexport", szFile, UI_BROWSE_SAVE, ".dat");
	}
	char szPicked[MAX_PATH];
	if (UiFileBrowserDraw("Export RomData Template###rdexport", szPicked, sizeof(szPicked))) {
		UiGameSelPauseScan();
		if (RomDataExportTemplate(nExportDrv, szPicked)) {
			AppError((TCHAR*)"The template couldn't be written.\n", 0);
		} else {
			char szText[MAX_PATH + 32];
			snprintf(szText, sizeof(szText), "Template written to %s", szPicked);
			UpdateMessage(szText);
		}
	}
}

void UiRomDataDraw()
{
	DrawExport();

	if (bRequest) {
		bRequest = false;
		Scan();
		ImGui::OpenPopup("RomData Manager###romdata");
	}

	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(UiDialogSize(960.0f, vp->WorkSize.y * 0.85f), ImGuiCond_Appearing);
	if (!ImGui::BeginPopupModal("RomData Manager###romdata", NULL, ImGuiWindowFlags_NoSavedSettings)) {
		if (nImagesFor >= 0 && !ImGui::IsPopupOpen("RomData Manager###romdata")) {
			FreeImages();
		}
		return;
	}

	ImGui::AlignTextToFramePadding();
	ImGui::Text("Dats in %s", UiText(szAppRomdataPath));
	ImGui::SameLine();
	if (ImGui::Checkbox("Subfolders", &bUiRomDataSubdirs)) {
		Scan();
	}
	ImGui::SameLine();
	if (ImGui::Button("Rescan")) {
		Scan();
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(the folder is in Misc > Support Paths)");

	float fBottom = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
	float fMain = ImGui::GetContentRegionAvail().y - fBottom;
	if (ImGui::BeginTable("##layout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV, ImVec2(0.0f, fMain))) {
		ImGui::TableSetupColumn("##list", ImGuiTableColumnFlags_WidthStretch, 1.7f);
		ImGui::TableSetupColumn("##info", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if (!Dats.empty()) {
			DrawList(fMain - ImGui::GetStyle().CellPadding.y * 2.0f);
		} else {
			ImGui::TextWrapped("No RomData dat in %s.", UiText(szAppRomdataPath));
		}
		ImGui::TableNextColumn();
		ImGui::BeginChild("##details", ImVec2(0.0f, fMain - ImGui::GetStyle().CellPadding.y * 2.0f));
		DrawDetails();
		ImGui::EndChild();
		ImGui::EndTable();
	}

	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("%d dats", (int)Dats.size());
	float fButton = 120.0f * fUiScale;
	ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - fButton * 2.0f - ImGui::GetStyle().ItemSpacing.x);
	ImGui::BeginDisabled(nSelected < 0 || Dats[nSelected < 0 ? 0 : nSelected].nDrv < 0);
	if (ImGui::Button("Play", ImVec2(fButton, 0.0f))) {
		if (UiRomDataPlay(Dats[nSelected].szDat)) {
			ImGui::CloseCurrentPopup();
		}
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Close", ImVec2(fButton, 0.0f)) || UiEscapePressed()) {
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}
