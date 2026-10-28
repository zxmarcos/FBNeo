// Game selector. This is the first version (list, search, availability and
// clone filters, rom scan), the full selector of the plan's phase 1 replaces it.
#include "burner.h"
#include "ui.h"
#include "imgui.h"

static char* gameAv = NULL;					// 0 = missing, 1 = roms with problems, 3 = ok (same as sdl2_gui.cpp)
static int* pnList = NULL;					// drivers shown, in driver order
static int nListCount = 0;
static int nAvailable = 0;
static int nSelected = -1;
static bool bScrollToSelected = false;
static bool bNeedFilter = true;
static int nScanPos = -1;					// next driver to check, -1 when no scan runs
static bool bOverGame = false;				// opened over the running game
static char szSearch[128] = "";

// ----------------------------------------------------------------------------
// Rom availability cache, the file the old selector uses

static void CreateRomDatName(char* szRomDat)
{
#if defined(BUILD_SDL2) && !defined(SDL_WINDOWS)
	char* pszConfigPath = SDL_GetPrefPath("fbneo", "config");
	snprintf(szRomDat, MAX_PATH, "%sroms.found", pszConfigPath);
	SDL_free(pszConfigPath);
#else
	snprintf(szRomDat, MAX_PATH, "fbneo.dat");
#endif
}

static int WriteGameAvb()
{
	char szRomDat[MAX_PATH];
	CreateRomDatName(szRomDat);

	FILE* h = fopen(szRomDat, "wt");
	if (h == NULL) {
		return 1;
	}

	fprintf(h, APP_TITLE " v%.20s ROMs", szAppBurnVer);
	fprintf(h, " 0x%04X ", nBurnDrvCount);
	for (UINT32 i = 0; i < nBurnDrvCount; i++) {
		fputc((gameAv[i] & 2) ? '*' : ((gameAv[i] & 1) ? '+' : '-'), h);
	}
	fprintf(h, " END");
	fclose(h);

	return 0;
}

static int ReadGameAvb()
{
	char szRomDat[MAX_PATH];
	CreateRomDatName(szRomDat);

	FILE* h = fopen(szRomDat, "rt");
	if (h == NULL) {
		return 1;
	}

	int nSize = nBurnDrvCount + 256;
	char* pszBuffer = (char*)calloc(nSize, 1);
	if (pszBuffer == NULL) {
		fclose(h);
		return 1;
	}
	fgets(pszBuffer, nSize, h);
	fclose(h);

	int nRet = 1;
	char szLabel[256];
	snprintf(szLabel, sizeof(szLabel), APP_TITLE " v%.20s ROMs", szAppBurnVer);
	char* p = LabelCheck(pszBuffer, szLabel);
	if (p) {
		char* pEnd = NULL;
		UINT32 nCount = strtol(p, &pEnd, 0);
		if (nCount == nBurnDrvCount && pEnd && *pEnd == ' ') {
			p = pEnd + 1;
			nRet = 0;
			for (UINT32 i = 0; i < nBurnDrvCount && nRet == 0; i++, p++) {
				switch (*p) {
					case '*': gameAv[i] = 3; break;
					case '+': gameAv[i] = 1; break;
					case '-': gameAv[i] = 0; break;
					default:  nRet = 1; break;
				}
			}
			if (nRet == 0 && strncmp(p, " END", 4) != 0) {
				nRet = 1;
			}
		}
	}
	free(pszBuffer);

	return nRet;
}

// ----------------------------------------------------------------------------

void UiGameSelRescan()
{
	if (UiGameRunning()) {
		return;
	}
	memset(gameAv, 0, nBurnDrvCount);
	nScanPos = 0;
	bNeedFilter = true;
}

// Checks the roms for a few milliseconds per frame, so the interface stays alive
static void ScanStep()
{
	if (nScanPos < 0) {
		return;
	}

	UINT32 nOldDrv = nBurnDrvActive;
	Uint32 nStart = SDL_GetTicks();
	while (nScanPos < (int)nBurnDrvCount && SDL_GetTicks() - nStart < 20) {
		nBurnDrvActive = nScanPos;
		switch (BzipOpen(true)) {
			case 0:  gameAv[nScanPos] = 3; break;
			case 2:  gameAv[nScanPos] = 1; break;
			default: gameAv[nScanPos] = 0; break;
		}
		BzipClose();
		nScanPos++;
	}
	nBurnDrvActive = nOldDrv;

	if (nScanPos >= (int)nBurnDrvCount) {
		nScanPos = -1;
		WriteGameAvb();
	}
	bNeedFilter = true;
}

static bool ContainsNoCase(const char* pszText, const char* pszWord, int nWordLen)
{
	if (pszText == NULL) {
		return false;
	}
	for (; *pszText; pszText++) {
		int i = 0;
		while (i < nWordLen && pszText[i] && tolower((unsigned char)pszText[i]) == tolower((unsigned char)pszWord[i])) {
			i++;
		}
		if (i == nWordLen) {
			return true;
		}
	}
	return false;
}

// Every word of the search must be found in one of the texts of the active driver
static bool SearchMatch()
{
	const char* p = szSearch;
	while (*p) {
		while (*p == ' ') p++;
		const char* pWord = p;
		while (*p && *p != ' ') p++;
		int nLen = p - pWord;
		if (nLen == 0) {
			continue;
		}
		if (!ContainsNoCase(BurnDrvGetTextA(DRV_FULLNAME), pWord, nLen) && !ContainsNoCase(BurnDrvGetTextA(DRV_NAME), pWord, nLen)
		 && !ContainsNoCase(BurnDrvGetTextA(DRV_MANUFACTURER), pWord, nLen) && !ContainsNoCase(BurnDrvGetTextA(DRV_SYSTEM), pWord, nLen)
		 && !ContainsNoCase(BurnDrvGetTextA(DRV_DATE), pWord, nLen)) {
			return false;
		}
	}
	return true;
}

static void DoFilter()
{
	UINT32 nOldDrv = nBurnDrvActive;

	nListCount = 0;
	nAvailable = 0;
	for (UINT32 i = 0; i < nBurnDrvCount; i++) {
		nBurnDrvActive = i;
		if (BurnDrvGetFlags() & BDF_BOARDROM) {
			continue;
		}
		if (gameAv[i]) {
			nAvailable++;
		}
		if (bShowAvailableOnly && !gameAv[i]) {
			continue;
		}
		if (!bShowClones && (BurnDrvGetFlags() & BDF_CLONE)) {
			continue;
		}
		if (szSearch[0] && !SearchMatch()) {
			continue;
		}
		pnList[nListCount++] = i;
	}

	nBurnDrvActive = nOldDrv;
	bNeedFilter = false;
}

static void GenreText(char* pszText, int nSize)
{
	static const struct { UINT32 nFlag; const char* pszName; } genres[] = {
		{ GBF_HORSHOOT, "Horizontal shooter" }, { GBF_VERSHOOT, "Vertical shooter" }, { GBF_SCRFIGHT, "Beat 'em up" },
		{ GBF_VSFIGHT, "Versus fighting" }, { GBF_BIOS, "BIOS" }, { GBF_BREAKOUT, "Breakout" }, { GBF_CASINO, "Casino" },
		{ GBF_BALLPADDLE, "Ball & paddle" }, { GBF_MAZE, "Maze" }, { GBF_MINIGAMES, "Mini-games" }, { GBF_PINBALL, "Pinball" },
		{ GBF_PLATFORM, "Platformer" }, { GBF_PUZZLE, "Puzzle" }, { GBF_QUIZ, "Quiz" }, { GBF_SPORTSMISC, "Sports" },
		{ GBF_SPORTSFOOTBALL, "Football" }, { GBF_MISC, "Misc" }, { GBF_MAHJONG, "Mahjong" }, { GBF_RACING, "Racing" },
		{ GBF_SHOOT, "Shooter" }, { GBF_ACTION, "Action" }, { GBF_RUNGUN, "Run 'n gun" }, { GBF_STRATEGY, "Strategy" },
		{ GBF_RPG, "RPG" }, { GBF_SIM, "Simulator" }, { GBF_ADV, "Adventure" },
	};

	UINT32 nFlags = BurnDrvGetGenreFlags();
	int n = 0;
	pszText[0] = '\0';
	for (unsigned int i = 0; i < sizeof(genres) / sizeof(genres[0]) && n < nSize; i++) {
		if (nFlags & genres[i].nFlag) {
			n += snprintf(pszText + n, nSize - n, "%s%s", n ? ", " : "", genres[i].pszName);
		}
	}
}

static void DrawInfo(int nDrv)
{
	if (nDrv < 0) {
		return;
	}

	UINT32 nOldDrv = nBurnDrvActive;
	nBurnDrvActive = nDrv;

	ImGui::TextUnformatted(UiText(BurnDrvGetTextA(DRV_FULLNAME)));
	if (BurnDrvGetTextA(DRV_PARENT)) {
		ImGui::Text("%s, clone of %s", BurnDrvGetTextA(DRV_NAME), BurnDrvGetTextA(DRV_PARENT));
	} else {
		ImGui::TextUnformatted(BurnDrvGetTextA(DRV_NAME));
	}
	ImGui::Text("%s, %s, %s hardware", UiText(BurnDrvGetTextA(DRV_MANUFACTURER)), BurnDrvGetTextA(DRV_DATE), UiText(BurnDrvGetTextA(DRV_SYSTEM)));

	char szGenre[256];
	GenreText(szGenre, sizeof(szGenre));
	int nPlayers = BurnDrvGetMaxPlayers();
	ImGui::Text("%d player%s%s%s", nPlayers, nPlayers == 1 ? "" : "s", szGenre[0] ? " - " : "", szGenre);

	const char* pszStatus = gameAv[nDrv] == 3 ? "ROMs OK" : (gameAv[nDrv] ? "ROMs with problems" : "ROMs missing");
	ImGui::TextColored(gameAv[nDrv] == 3 ? ImVec4(0.5f, 0.9f, 0.5f, 1.0f) : ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s", pszStatus);
	if (!BurnDrvIsWorking()) {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), " - not working");
	}
	if (BurnDrvGetTextA(DRV_COMMENT)) {
		ImGui::SameLine();
		ImGui::TextDisabled(" - %s", UiText(BurnDrvGetTextA(DRV_COMMENT)));
	}

	nBurnDrvActive = nOldDrv;
}

void UiGameSelDraw(bool bOverRunningGame)
{
	static bool bGameWasRunning = false;
	if (!bOverRunningGame && bGameWasRunning) {
		nSelected = nGameSelect;		// back from a game
		bScrollToSelected = true;
	}
	bGameWasRunning = bOverRunningGame;

	if (bOverRunningGame && !bOverGame) {
		return;
	}
	if (!bOverRunningGame) {
		bOverGame = false;
		ScanStep();
	}
	if (bNeedFilter) {
		DoFilter();
	}

	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGuiStyle& style = ImGui::GetStyle();
	ImGuiWindowFlags nFlags = ImGuiWindowFlags_NoSavedSettings;
	bool bOpen = true;

	if (bOverRunningGame) {
		ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x * 0.85f, vp->WorkSize.y * 0.85f), ImGuiCond_Appearing);
		nFlags |= ImGuiWindowFlags_NoCollapse;
	} else {
		ImGui::SetNextWindowPos(vp->WorkPos);
		ImGui::SetNextWindowSize(vp->WorkSize);
		nFlags |= ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
	}

	int nLaunch = -1;

	if (ImGui::Begin(bOverRunningGame ? "Load Game###gamesel" : "##gamesel", bOverRunningGame ? &bOpen : NULL, nFlags)) {
		if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F)) {
			ImGui::SetKeyboardFocusHere();
		}
		ImGui::SetNextItemWidth(340.0f * fUiScale);
		if (ImGui::InputTextWithHint("##search", "Search (Ctrl+F): name, zip, maker, year, system", szSearch, sizeof(szSearch))) {
			bNeedFilter = true;
			bScrollToSelected = true;
		}
		ImGui::SameLine();
		if (ImGui::Checkbox("Available only", &bShowAvailableOnly)) {
			bNeedFilter = true;
			bScrollToSelected = true;
		}
		ImGui::SameLine();
		if (ImGui::Checkbox("Clones", &bShowClones)) {
			bNeedFilter = true;
			bScrollToSelected = true;
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(UiGameRunning() || nScanPos >= 0);
		if (ImGui::Button("Rescan ROMs")) {
			UiGameSelRescan();
		}
		ImGui::EndDisabled();

		if (nScanPos >= 0) {
			char szScan[64];
			snprintf(szScan, sizeof(szScan), "Scanning ROMs... %d of %d", nScanPos, nBurnDrvCount);
			ImGui::ProgressBar((float)nScanPos / nBurnDrvCount, ImVec2(-1.0f, 0.0f), szScan);
		}

		// room for the game information below the list
		float fInfo = ImGui::GetTextLineHeightWithSpacing() * 5.0f + ImGui::GetFrameHeightWithSpacing() + style.ItemSpacing.y * 2.0f;
		ImGuiTableFlags nTableFlags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable;

		if (ImGui::BeginTable("##games", 5, nTableFlags, ImVec2(0.0f, -fInfo))) {
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Zip", ImGuiTableColumnFlags_WidthFixed, 90.0f * fUiScale);
			ImGui::TableSetupColumn("Year", ImGuiTableColumnFlags_WidthFixed, 50.0f * fUiScale);
			ImGui::TableSetupColumn("Manufacturer", ImGuiTableColumnFlags_WidthFixed, 190.0f * fUiScale);
			ImGui::TableSetupColumn("System", ImGuiTableColumnFlags_WidthFixed, 190.0f * fUiScale);
			ImGui::TableHeadersRow();

			int nSelectedRow = -1;
			if (bScrollToSelected) {
				for (int i = 0; i < nListCount; i++) {
					if (pnList[i] == nSelected) {
						nSelectedRow = i;
						break;
					}
				}
			}

			UINT32 nOldDrv = nBurnDrvActive;
			ImGuiListClipper clipper;
			clipper.Begin(nListCount);
			if (nSelectedRow >= 0) {
				clipper.IncludeItemByIndex(nSelectedRow);
			}
			while (clipper.Step()) {
				for (int nRow = clipper.DisplayStart; nRow < clipper.DisplayEnd; nRow++) {
					int nDrv = pnList[nRow];
					nBurnDrvActive = nDrv;

					ImU32 nColor = ImGui::GetColorU32(ImGuiCol_Text);
					if (!gameAv[nDrv]) {
						nColor = IM_COL32(170, 90, 90, 255);
					} else if (BurnDrvGetFlags() & BDF_CLONE) {
						nColor = IM_COL32(170, 180, 190, 255);
					}

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::PushStyleColor(ImGuiCol_Text, nColor);
					ImGui::PushID(nDrv);

					bool bSelected = nDrv == nSelected;
					if (ImGui::Selectable(UiText(BurnDrvGetTextA(DRV_FULLNAME)), bSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
						nSelected = nDrv;
						// a double click, or Enter / the gamepad button: a single click only selects
						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) || !ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
							nLaunch = nDrv;
						}
					}
					if (ImGui::IsItemFocused()) {
						nSelected = nDrv;		// follows the keyboard / gamepad
					}
					if (nRow == nSelectedRow) {
						ImGui::SetScrollHereY(0.5f);
						bScrollToSelected = false;
					}

					ImGui::PopID();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(BurnDrvGetTextA(DRV_NAME));
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(BurnDrvGetTextA(DRV_DATE));
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(UiText(BurnDrvGetTextA(DRV_MANUFACTURER)));
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(UiText(BurnDrvGetTextA(DRV_SYSTEM)));
					ImGui::PopStyleColor();
				}
			}
			nBurnDrvActive = nOldDrv;
			bScrollToSelected = false;

			ImGui::EndTable();
		}

		ImGui::BeginChild("##info", ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 5.0f));
		DrawInfo(nSelected);
		ImGui::EndChild();

		ImGui::BeginDisabled(nSelected < 0);
		if (ImGui::Button("Play", ImVec2(120.0f * fUiScale, 0.0f))) {
			nLaunch = nSelected;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::TextDisabled("Showing %d games, %d of %d available", nListCount, nAvailable, nBurnDrvCount);
	}
	ImGui::End();

	if (!bOpen) {
		bOverGame = false;
	}
	if (nLaunch >= 0) {
		bOverGame = false;
		UiLaunchGame(nLaunch);
	}
}

bool UiGameSelIsOpen()
{
	return bOverGame;
}

void UiGameSelClose()
{
	bOverGame = false;
}

void UiGameSelOpen()
{
	bOverGame = true;
	if (UiGameRunning()) {
		nSelected = nBurnDrvActive;
	}
	bScrollToSelected = true;
}

void UiGameSelInit()
{
	gameAv = (char*)calloc(nBurnDrvCount, 1);
	pnList = (int*)malloc(nBurnDrvCount * sizeof(int));

	nSelected = (nGameSelect >= 0 && (UINT32)nGameSelect < nBurnDrvCount) ? nGameSelect : -1;
	bScrollToSelected = true;
	bNeedFilter = true;

	if (ReadGameAvb()) {
		UiGameSelRescan();
	}
}

void UiGameSelExit()
{
	if (nSelected >= 0) {
		nGameSelect = nSelected;
	}

	free(gameAv);
	gameAv = NULL;
	free(pnList);
	pnList = NULL;
}
