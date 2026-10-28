// IPS manager (win32/ips_manager.cpp): the patches of a game by category,
// their description in the chosen language and their preview, saved to
// config/ips/<game>.ini. The patching itself is in ../ips_manager.cpp.
#include "burner.h"
#include "gamelist_model.h"
#include "ui.h"
#include "imgui.h"
#include <dirent.h>
#include <vector>
#include <string>
#include <algorithm>

struct IpsPatch {
	std::string Dat;					// the path of the dat
	std::string Category;				// before the '/' of the first line of the description, or empty
	std::string Name;
	bool bChecked;
};

static bool bManagerRequest = false;
static int nManagerDrv = -1;
static std::vector<IpsPatch> Patches;
static int nPatchSelected = -1;
static char* pszDesc = NULL;			// of the selected patch
static SDL_Texture* pPreview = NULL;
static int nPreviewW = 0, nPreviewH = 0;

static bool bLaunchArmed = false;		// the next game loads with its active patches

static bool IsDat(const char* pszName)
{
	int nLen = strlen(pszName);
	return nLen > 4 && _stricmp(pszName + nLen - 4, ".dat") == 0;
}

bool UiIpsHasPatches(int nDrv)
{
	const GameListEntry* e = GameListGet(nDrv);
	if (e == NULL) {
		return false;
	}
	DIR* d = opendir(IpsPatchFolder(e->pszName));
	if (d == NULL) {
		return false;
	}
	bool bFound = false;
	struct dirent* ent;
	while (!bFound && (ent = readdir(d)) != NULL) {
		bFound = IsDat(ent->d_name);
	}
	closedir(d);
	return bFound;
}

// The saved patches of the game, without touching nBurnDrvActive for long
int UiIpsActiveCount(int nDrv)
{
	if (nDrv < 0 || (UINT32)nDrv >= nBurnDrvCount) {
		return 0;
	}
	UiGameSelPauseScan();
	UINT32 nOldDrv = nBurnDrvActive;
	nBurnDrvActive = nDrv;
	int nCount = LoadIpsActivePatches();
	nBurnDrvActive = nOldDrv;
	return nCount;
}

void UiIpsArmLaunch(bool bArm)
{
	bLaunchArmed = bArm;
}

// From StartGame(), with nBurnDrvActive set: the patches go with the load
void UiIpsBeforeLoad()
{
	bool bArmed = bLaunchArmed;
	bLaunchArmed = false;
	IpsPatchExit();
	if (bArmed && LoadIpsActivePatches() > 0) {
		bDoIpsPatch = true;
		IpsPatchInit();
	}
}

// ----------------------------------------------------------------------------

static void FreeSelection()
{
	free(pszDesc);
	pszDesc = NULL;
	if (pPreview) {
		SDL_DestroyTexture(pPreview);
	}
	pPreview = NULL;
	nPreviewW = nPreviewH = 0;
	nPatchSelected = -1;
}

static void SelectPatch(int i)
{
	FreeSelection();
	nPatchSelected = i;
	if (i < 0) {
		return;
	}
	pszDesc = IpsPatchDesc(Patches[i].Dat.c_str(), nIpsSelectedLanguage);

	// the preview is the png next to the dat
	std::string Png = Patches[i].Dat.substr(0, Patches[i].Dat.size() - 3) + "png";
	pPreview = UiImageLoad(Png.c_str(), &nPreviewW, &nPreviewH);
}

// The names of the patches: the first line of the description, "Category/Name"
static void LoadNames()
{
	for (size_t i = 0; i < Patches.size(); i++) {
		IpsPatch& p = Patches[i];
		char* pszText = IpsPatchDesc(p.Dat.c_str(), nIpsSelectedLanguage);
		std::string First;
		if (pszText) {
			const char* pEnd = strchr(pszText, '\n');
			First.assign(pszText, pEnd ? pEnd - pszText : strlen(pszText));
			free(pszText);
		}
		if (First.empty()) {
			size_t nSlash = p.Dat.find_last_of('/');
			First = p.Dat.substr(nSlash == std::string::npos ? 0 : nSlash + 1);
		}
		size_t nSep = First.find('/');
		if (nSep != std::string::npos && nSep > 0 && nSep + 1 < First.size()) {
			p.Category = First.substr(0, nSep);
			p.Name = First.substr(nSep + 1);
		} else {
			p.Category.clear();
			p.Name = First;
		}
	}
}

static bool PatchLess(const IpsPatch& a, const IpsPatch& b)
{
	return a.Dat < b.Dat;
}

static void LoadPatches()
{
	Patches.clear();
	FreeSelection();

	const GameListEntry* e = GameListGet(nManagerDrv);
	const char* pszFolder = IpsPatchFolder(e->pszName);
	DIR* d = opendir(pszFolder);
	if (d) {
		struct dirent* ent;
		while ((ent = readdir(d)) != NULL) {
			if (IsDat(ent->d_name)) {
				IpsPatch p;
				p.Dat = std::string(pszFolder) + ent->d_name;
				p.bChecked = false;
				Patches.push_back(p);
			}
		}
		closedir(d);
	}
	std::sort(Patches.begin(), Patches.end(), PatchLess);
	LoadNames();

	int nActive = UiIpsActiveCount(nManagerDrv);
	for (int a = 0; a < nActive; a++) {
		const char* pszActive = IpsActivePatch(a);
		for (size_t i = 0; i < Patches.size(); i++) {
			if (_stricmp(Patches[i].Dat.c_str(), pszActive) == 0) {
				Patches[i].bChecked = true;
			}
		}
	}
}

void UiIpsOpen(int nDrv)
{
	if (nDrv >= 0 && (UINT32)nDrv < nBurnDrvCount) {
		nManagerDrv = nDrv;
		bManagerRequest = true;
	}
}

static void PatchRow(int i)
{
	IpsPatch& p = Patches[i];
	ImGui::PushID(i);
	ImGui::Checkbox("##on", &p.bChecked);
	ImGui::SameLine();
	if (ImGui::Selectable(UiText(p.Name.c_str()), nPatchSelected == i)) {
		SelectPatch(i);
	}
	ImGui::PopID();
}

static void DrawTree()
{
	// the patches without a category, then each category
	for (size_t i = 0; i < Patches.size(); i++) {
		if (Patches[i].Category.empty()) {
			PatchRow((int)i);
		}
	}
	std::vector<std::string> Done;
	for (size_t i = 0; i < Patches.size(); i++) {
		const std::string& Cat = Patches[i].Category;
		if (Cat.empty() || std::find(Done.begin(), Done.end(), Cat) != Done.end()) {
			continue;
		}
		Done.push_back(Cat);
		ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
		if (ImGui::TreeNode(Cat.c_str(), "%s", UiText(Cat.c_str()))) {
			for (size_t j = i; j < Patches.size(); j++) {
				if (Patches[j].Category == Cat) {
					PatchRow((int)j);
				}
			}
			ImGui::TreePop();
		}
	}
	if (Patches.empty()) {
		ImGui::TextDisabled("No patches in %s", IpsPatchFolder(GameListGet(nManagerDrv)->pszName));
	}
}

static void DrawDetails(float fHeight)
{
	float fImage = fHeight * 0.5f;
	ImVec2 box(ImGui::GetContentRegionAvail().x, fImage);
	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(pos, ImVec2(pos.x + box.x, pos.y + box.y), IM_COL32(0, 0, 0, 120));
	if (pPreview && nPreviewW > 0 && nPreviewH > 0) {
		float fScale = box.x / nPreviewW;
		if (nPreviewH * fScale > box.y) fScale = box.y / nPreviewH;
		ImVec2 size(nPreviewW * fScale, nPreviewH * fScale);
		ImVec2 at(pos.x + (box.x - size.x) * 0.5f, pos.y + (box.y - size.y) * 0.5f);
		dl->AddImage((ImTextureID)(intptr_t)pPreview, at, ImVec2(at.x + size.x, at.y + size.y));
	} else {
		const char* pszText = "No preview";
		ImVec2 size = ImGui::CalcTextSize(pszText);
		dl->AddText(ImVec2(pos.x + (box.x - size.x) * 0.5f, pos.y + (box.y - size.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), pszText);
	}
	ImGui::Dummy(box);

	// a table cell doesn't limit the height, so the box is sized to what's left of it
	ImGui::BeginChild("##desc", ImVec2(0.0f, fHeight - fImage - ImGui::GetStyle().ItemSpacing.y), ImGuiChildFlags_Borders);
	if (pszDesc) {
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextUnformatted(pszDesc);
		ImGui::PopTextWrapPos();
	} else {
		ImGui::TextDisabled("%s", nPatchSelected >= 0 ? "No description" : "Select a patch to see its description.");
	}
	ImGui::EndChild();
}

static void Save()
{
	const GameListEntry* e = GameListGet(nManagerDrv);
	std::vector<const char*> Dats;
	for (size_t i = 0; i < Patches.size(); i++) {
		if (Patches[i].bChecked) {
			Dats.push_back(Patches[i].Dat.c_str());
		}
	}
	if (IpsSaveActivePatches(e->pszName, e->pszFullName, Dats.empty() ? NULL : &Dats[0], (INT32)Dats.size())) {
		FBAPopupAddText(PUF_TEXT_NO_TRANSLATE, _T("Couldn't write the IPS config of %s.\n"), e->pszName);
		FBAPopupDisplay(PUF_TYPE_ERROR);
	}
}

void UiIpsDraw()
{
	if (bManagerRequest) {
		bManagerRequest = false;
		LoadPatches();
		ImGui::OpenPopup("###ipsmanager");
	}
	if (nManagerDrv < 0) {
		return;
	}

	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(UiDialogSize(860.0f, vp->WorkSize.y * 0.8f), ImGuiCond_Appearing);

	char szTitle[300];
	snprintf(szTitle, sizeof(szTitle), "IPS Patches: %s###ipsmanager", UiText(GameListGet(nManagerDrv)->pszFullName));
	if (!ImGui::BeginPopupModal(szTitle, NULL, ImGuiWindowFlags_NoSavedSettings)) {
		if (!ImGui::IsPopupOpen("###ipsmanager")) {
			FreeSelection();
			Patches.clear();
			nManagerDrv = -1;
		}
		return;
	}

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Language:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(220.0f * fUiScale);
	if (ImGui::BeginCombo("##lang", IpsLanguageNames[nIpsSelectedLanguage])) {
		for (int i = 0; i < IPS_LANGUAGES; i++) {
			if (ImGui::Selectable(IpsLanguageNames[i], nIpsSelectedLanguage == i)) {
				nIpsSelectedLanguage = i;
				int nSel = nPatchSelected;
				LoadNames();
				SelectPatch(nSel);
			}
		}
		ImGui::EndCombo();
	}

	float fBottom = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
	float fMain = ImGui::GetContentRegionAvail().y - fBottom;
	if (ImGui::BeginTable("##ips", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV, ImVec2(0.0f, fMain))) {
		ImGui::TableSetupColumn("##tree", ImGuiTableColumnFlags_WidthStretch, 1.1f);
		ImGui::TableSetupColumn("##details", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::BeginChild("##list", ImVec2(0.0f, fMain - ImGui::GetStyle().CellPadding.y * 2.0f));
		DrawTree();
		ImGui::EndChild();
		ImGui::TableNextColumn();
		DrawDetails(fMain - ImGui::GetStyle().CellPadding.y * 2.0f);
		ImGui::EndTable();
	}

	float fButton = 120.0f * fUiScale;
	if (ImGui::Button("Deselect All", ImVec2(fButton, 0.0f))) {
		for (size_t i = 0; i < Patches.size(); i++) {
			Patches[i].bChecked = false;
		}
	}
	ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - fButton * 2.0f - ImGui::GetStyle().ItemSpacing.x);
	if (ImGui::Button("OK", ImVec2(fButton, 0.0f))) {
		Save();
		UiGameSelRefresh();
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(fButton, 0.0f)) || UiEscapePressed()) {
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}
