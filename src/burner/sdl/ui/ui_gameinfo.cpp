// Game Info (win32/gameinfo.cpp): the roms, the HDD images and the samples of
// a set, its history.xml and command.dat entries and its images
#include "burner.h"
#include "gamelist_model.h"
#include "gameinfo_text.h"
#include "ui.h"
#include "imgui.h"
#include <vector>
#include <string>

struct RomRow {
	std::string Name;
	UINT32 nLen;
	UINT32 nCrc;
	UINT32 nType;
	bool bBoard;						// from the board rom set (DRV_BOARDROM)
};

// the tabs of the images, after the text ones
static const struct { const char* pszTab; TCHAR* pszPath; const char* pszZip; } ImageTabs[] = {
	{ "In Game",	szAppPreviewsPath,	"previews" },
	{ "Title",		szAppTitlesPath,	"titles" },
	{ "Select",		szAppSelectPath,	"select" },
	{ "Versus",		szAppVersusPath,	"versus" },
	{ "How To",		szAppHowtoPath,		"howto" },
	{ "Scores",		szAppScoresPath,	"scores" },
	{ "Bosses",		szAppBossesPath,	"bosses" },
	{ "Game Over",	szAppGameoverPath,	"gameover" },
	{ "Flyer",		szAppFlyersPath,	"flyers" },
	{ "Cabinet",	szAppCabinetsPath,	"cabinets" },
	{ "Marquee",	szAppMarqueesPath,	"marquees" },
	{ "Controls",	szAppControlsPath,	"cpanel" },
	{ "PCB",		szAppPCBsPath,		"pcbs" },
};
#define IMAGE_TABS ((int)(sizeof(ImageTabs) / sizeof(ImageTabs[0])))

static bool bInfoRequest = false;
static int nInfoDrv = -1;
static std::vector<RomRow> Roms;
static std::vector<std::string> Samples;
static std::vector<std::string> Hdds;
static char szInfoText[6][1024];

static char* pszHistory = NULL;
static char* pszCommands = NULL;
static bool bHistoryLoaded = false;
static bool bCommandsLoaded = false;

static int nImageTab = -1;				// the image loaded
static SDL_Texture* pImage = NULL;
static int nImageW = 0, nImageH = 0;

static int nRescanResult = -1;			// GAMELIST_AV_*, -1 not rescanned
static char szRescanText[2048];

void UiGameInfoOpen(int nDrv)
{
	if (nDrv >= 0 && (UINT32)nDrv < nBurnDrvCount) {
		nInfoDrv = nDrv;
		bInfoRequest = true;
	}
}

static void FreeImage()
{
	if (pImage) {
		SDL_DestroyTexture(pImage);
	}
	pImage = NULL;
	nImageW = nImageH = 0;
	nImageTab = -1;
}

static void FreeTexts()
{
	free(pszHistory);
	free(pszCommands);
	pszHistory = pszCommands = NULL;
	bHistoryLoaded = bCommandsLoaded = false;
}

// <path><name>.png, the parent's, then <path><zip>.zip and its <zip>/<name>.png
static void LoadImage(int nTab)
{
	FreeImage();
	nImageTab = nTab;

	const GameListEntry* e = GameListGet(nInfoDrv);
	const char* pszNames[2] = { e->pszName, e->nParent >= 0 ? e->pszParent : NULL };
	const char* pszPath = ImageTabs[nTab].pszPath;
	char szFile[MAX_PATH * 2];

	for (int i = 0; i < 2 && pszNames[i] && pImage == NULL; i++) {
		snprintf(szFile, sizeof(szFile), "%s%s.png", pszPath, pszNames[i]);
		pImage = UiImageLoad(szFile, &nImageW, &nImageH);
	}
	if (pImage == NULL) {
		char szZip[MAX_PATH * 2];
		snprintf(szZip, sizeof(szZip), "%s%s.zip", pszPath, ImageTabs[nTab].pszZip);
		UiZip* pZip = UiZipOpen(szZip);
		for (int i = 0; i < 2 && pszNames[i] && pZip && pImage == NULL; i++) {
			snprintf(szFile, sizeof(szFile), "%s/%s.png", ImageTabs[nTab].pszZip, pszNames[i]);
			pImage = UiImageLoadZip(pZip, szFile, &nImageW, &nImageH);
		}
		UiZipClose(pZip);
	}
}

static void AddRoms(bool bBoard)
{
	for (int i = 0; i < 0x100; i++) {
		struct BurnRomInfo ri;
		char* pszName = NULL;
		memset(&ri, 0, sizeof(ri));
		if (BurnDrvGetRomInfo(&ri, i)) {
			continue;						// the board roms of STDROMPICKEXT() start at 0x80
		}
		BurnDrvGetRomName(&pszName, i, 0);
		if (ri.nLen == 0 || pszName == NULL) {
			continue;
		}
		if (!bBoard && (ri.nType & BRF_BIOS) && i >= 0x80) {
			continue;						// the bios of the board, listed with the board roms
		}
		RomRow r;
		r.Name = pszName;
		r.nLen = ri.nLen;
		r.nCrc = ri.nCrc;
		r.nType = ri.nType;
		r.bBoard = bBoard;
		Roms.push_back(r);
	}
}

// what BurnDrvGet*() need nBurnDrvActive for, at once
static void LoadLists()
{
	Roms.clear();
	Samples.clear();
	Hdds.clear();

	UiGameSelPauseScan();
	UINT32 nOldDrv = nBurnDrvActive;
	nBurnDrvActive = nInfoDrv;

	AddRoms(false);
	const char* pszBoard = BurnDrvGetTextA(DRV_BOARDROM);
	if (pszBoard) {
		int nBoard = GameListFind(pszBoard);
		if (nBoard >= 0) {
			nBurnDrvActive = nBoard;
			AddRoms(true);
			nBurnDrvActive = nInfoDrv;
		}
	}

	if (BurnDrvGetTextA(DRV_SAMPLENAME)) {
		for (int i = 0; i < 0x100; i++) {
			struct BurnSampleInfo si;
			char* pszName = NULL;
			memset(&si, 0, sizeof(si));
			if (BurnDrvGetSampleInfo(&si, i)) {
				continue;
			}
			BurnDrvGetSampleName(&pszName, i, 0);
			if (si.nFlags && pszName) {
				Samples.push_back(pszName);
			}
		}
	}

	for (int i = 0; i < 0x100; i++) {
		char* pszName = NULL;
		if (BurnDrvGetHDDName(&pszName, i, 0) || pszName == NULL) {
			break;
		}
		Hdds.push_back(pszName);
	}

	nBurnDrvActive = nOldDrv;

	GameListInfoTitles(nInfoDrv, szInfoText[0], sizeof(szInfoText[0]));
	GameListInfoRom(nInfoDrv, szInfoText[1], sizeof(szInfoText[1]));
	GameListInfoBoard(nInfoDrv, szInfoText[2], sizeof(szInfoText[2]));
	GameListInfoRelease(nInfoDrv, szInfoText[3], sizeof(szInfoText[3]));
	GameListInfoGenre(nInfoDrv, szInfoText[4], sizeof(szInfoText[4]));
	GameListInfoNotes(nInfoDrv, szInfoText[5], sizeof(szInfoText[5]));
}

static void Rescan()
{
	nRescanResult = UiGameSelCheckRoms(nInfoDrv, szRescanText, sizeof(szRescanText));
}

// ----------------------------------------------------------------------------

static void DrawHeader()
{
	static const char* pszLabels[6] = { "Game:", "ROM:", "Info:", "Released:", "Genre:", "Notes:" };

	if (!ImGui::BeginTable("##header", 2, ImGuiTableFlags_SizingFixedFit)) {
		return;
	}
	ImGui::TableSetupColumn("##label");
	ImGui::TableSetupColumn("##text", ImGuiTableColumnFlags_WidthStretch);
	for (int i = 0; i < 6; i++) {
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextDisabled("%s", pszLabels[i]);
		ImGui::TableNextColumn();
		ImGui::TextWrapped("%s", UiText(szInfoText[i]));
	}
	ImGui::EndTable();
}

static void DrawRoms()
{
	ImGuiTableFlags nFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;
	if (!ImGui::BeginTable("##roms", 5, nFlags)) {
		return;
	}
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Size (bytes)");
	ImGui::TableSetupColumn("CRC32");
	ImGui::TableSetupColumn("Type");
	ImGui::TableSetupColumn("Flags");
	ImGui::TableHeadersRow();

	for (size_t i = 0; i < Roms.size(); i++) {
		const RomRow& r = Roms[i];
		char szType[100] = "";
		static const struct { UINT32 nFlag; const char* pszName; } Types[] = {
			{ BRF_ESS, "Essential" }, { BRF_OPT, "Optional" }, { BRF_PRG, "Program" }, { BRF_GRA, "Graphics" }, { BRF_SND, "Sound" }, { BRF_BIOS, "BIOS" },
		};
		for (unsigned int t = 0; t < sizeof(Types) / sizeof(Types[0]); t++) {
			if (r.nType & Types[t].nFlag) {
				int nLen = strlen(szType);
				snprintf(szType + nLen, sizeof(szType) - nLen, "%s%s", nLen ? ", " : "", Types[t].pszName);
			}
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if (r.bBoard) {
			ImGui::TextDisabled("%s", r.Name.c_str());
		} else {
			ImGui::TextUnformatted(r.Name.c_str());
		}
		ImGui::TableNextColumn();
		ImGui::Text("%u", r.nLen);
		ImGui::TableNextColumn();
		if (!(r.nType & BRF_NODUMP)) {
			ImGui::Text("%08X", r.nCrc);
		}
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(szType);
		ImGui::TableNextColumn();
		if (r.nType & BRF_NODUMP) {
			ImGui::TextUnformatted("No Dump");
		}
	}
	ImGui::EndTable();
}

static void DrawNames(const char* pszId, const std::vector<std::string>& Names)
{
	if (ImGui::BeginTable(pszId, 1, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Name");
		ImGui::TableHeadersRow();
		for (size_t i = 0; i < Names.size(); i++) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(Names[i].c_str());
		}
		ImGui::EndTable();
	}
}

// the title, the "- SECTION -" headings, and the rest
static void DrawText(const char* pszText, const char* pszMissing)
{
	ImGui::BeginChild("##text", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
	if (pszText == NULL) {
		ImGui::TextDisabled("%s", pszMissing);
	} else {
		ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram);
		ImGui::PushTextWrapPos(0.0f);
		bool bFirst = true;
		for (const char* p = pszText; *p; ) {
			const char* pEnd = strchr(p, '\n');
			if (pEnd == NULL) pEnd = p + strlen(p);
			if (pEnd == p) {
				ImGui::Spacing();
			} else if (bFirst || (pEnd - p > 1 && p[0] == '-' && p[1] == ' ')) {
				ImGui::PushStyleColor(ImGuiCol_Text, accent);
				ImGui::TextUnformatted(p, pEnd);
				ImGui::PopStyleColor();
			} else {
				ImGui::TextUnformatted(p, pEnd);
			}
			bFirst = false;
			p = (*pEnd == '\n') ? pEnd + 1 : pEnd;
		}
		ImGui::PopTextWrapPos();
	}
	ImGui::EndChild();
}

static void DrawImageTab(int nTab)
{
	if (nImageTab != nTab) {
		LoadImage(nTab);
	}

	ImVec2 box = ImGui::GetContentRegionAvail();
	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(pos, ImVec2(pos.x + box.x, pos.y + box.y), IM_COL32(0, 0, 0, 120));

	if (pImage && nImageW > 0 && nImageH > 0) {
		float fScale = box.x / nImageW;
		if (nImageH * fScale > box.y) fScale = box.y / nImageH;
		ImVec2 size(nImageW * fScale, nImageH * fScale);
		ImVec2 at(pos.x + (box.x - size.x) * 0.5f, pos.y + (box.y - size.y) * 0.5f);
		dl->AddImage((ImTextureID)(intptr_t)pImage, at, ImVec2(at.x + size.x, at.y + size.y));
	} else {
		char szText[MAX_PATH + 64];
		snprintf(szText, sizeof(szText), "No image in %s", ImageTabs[nTab].pszPath);
		ImVec2 size = ImGui::CalcTextSize(szText);
		dl->AddText(ImVec2(pos.x + (box.x - size.x) * 0.5f, pos.y + (box.y - size.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), szText);
	}
	ImGui::Dummy(box);
}

static void DrawButtons()
{
	const GameListEntry* e = GameListGet(nInfoDrv);
	bool bFavorite = e->bFavorite;
	ImGui::AlignTextToFramePadding();
	if (ImGui::Checkbox("Favorite", &bFavorite)) {
		GameListFavoriteSet(nInfoDrv, bFavorite);
		UiGameSelRefresh();
	}
	ImGui::SameLine();
	// BzipOpen() would change the roms of the running game
	ImGui::BeginDisabled(UiGameRunning());
	if (ImGui::Button("Rescan Romset")) {
		Rescan();
	}
	ImGui::EndDisabled();
	if (nRescanResult >= 0) {
		ImGui::SameLine();
		if (nRescanResult == GAMELIST_AV_OK) {
			ImGui::TextUnformatted("Romset OK!");
		} else {
			ImGui::TextUnformatted(nRescanResult == GAMELIST_AV_OPTIONAL ? "Playable, some roms are missing or different" : "Essential roms are missing");
			if (szRescanText[0] && ImGui::IsItemHovered()) {
				ImGui::SetTooltip("%s", UiText(szRescanText));
			}
		}
	}

	float fButton = 120.0f * fUiScale;
	ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - fButton);
	if (ImGui::Button("Close", ImVec2(fButton, 0.0f)) || UiEscapePressed()) {
		ImGui::CloseCurrentPopup();
	}
}

void UiGameInfoDraw()
{
	if (bInfoRequest) {
		bInfoRequest = false;
		FreeImage();
		FreeTexts();
		nRescanResult = -1;
		szRescanText[0] = '\0';
		LoadLists();
		ImGui::OpenPopup("###gameinfo");
	}
	if (nInfoDrv < 0) {
		return;
	}

	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(UiDialogSize(820.0f, vp->WorkSize.y * 0.85f), ImGuiCond_Appearing);

	char szTitle[300];
	snprintf(szTitle, sizeof(szTitle), "Game Info: %s###gameinfo", UiText(GameListGet(nInfoDrv)->pszFullName));
	bool bOpen = true;
	if (!ImGui::BeginPopupModal(szTitle, &bOpen, ImGuiWindowFlags_NoSavedSettings)) {
		if (!ImGui::IsPopupOpen("###gameinfo")) {
			FreeImage();						// closed
			FreeTexts();
			nInfoDrv = -1;
		}
		return;
	}

	DrawHeader();
	ImGui::Spacing();

	float fBottom = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
	ImGui::BeginChild("##tabs", ImVec2(0.0f, -fBottom));
	if (ImGui::BeginTabBar("##tabbar", ImGuiTabBarFlags_FittingPolicyScroll)) {
		if (ImGui::BeginTabItem("ROMs")) {
			DrawRoms();
			ImGui::EndTabItem();
		}
		if (!Hdds.empty() && ImGui::BeginTabItem("HDD")) {
			DrawNames("##hdd", Hdds);
			ImGui::EndTabItem();
		}
		if (!Samples.empty() && ImGui::BeginTabItem("Samples")) {
			DrawNames("##samples", Samples);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("History")) {
			if (!bHistoryLoaded) {
				UiGameSelPauseScan();			// GameInfoHistory() uses nBurnDrvActive
				pszHistory = GameInfoHistory(nInfoDrv);
				bHistoryLoaded = true;
			}
			char szMissing[MAX_PATH + 64];
			snprintf(szMissing, sizeof(szMissing), "Nothing for this game in %shistory.xml", szAppHistoryPath);
			DrawText(pszHistory, szMissing);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Commands")) {
			if (!bCommandsLoaded) {
				UiGameSelPauseScan();
				pszCommands = GameInfoCommands(nInfoDrv);
				bCommandsLoaded = true;
			}
			char szMissing[MAX_PATH + 64];
			snprintf(szMissing, sizeof(szMissing), "Nothing for this game in %scommand.dat", szAppCommandPath);
			DrawText(pszCommands, szMissing);
			ImGui::EndTabItem();
		}
		for (int i = 0; i < IMAGE_TABS; i++) {
			if (ImGui::BeginTabItem(ImageTabs[i].pszTab)) {
				DrawImageTab(i);
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}
	ImGui::EndChild();

	DrawButtons();
	ImGui::EndPopup();
}
