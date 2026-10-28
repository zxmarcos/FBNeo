// Editors of the rom paths and of the support paths (Misc menu, selector)
#include "burner.h"
#include "ui.h"
#include "imgui.h"

static bool bRomPathsRequest = false;
static bool bSupportPathsRequest = false;

static char szEditRoms[DIRS_MAX][MAX_PATH];
static int nBrowseRow = -1;

static const struct { const char* pszName; TCHAR* pszPath; } SupportPaths[] = {
	{ "Previews",			szAppPreviewsPath },
	{ "Titles",				szAppTitlesPath },
	{ "Icons",				szAppIconsPath },
	{ "Cheats",				szAppCheatsPath },
	{ "High scores",		szAppHiscorePath },
	{ "Samples",			szAppSamplesPath },
	{ "HDD images",			szAppHDDPath },
	{ "IPS patches",		szAppIpsPath },
	{ "Blend tables",		szAppBlendPath },
	{ "EEPROM / NVRAM",		szAppEEPROMPath },
	{ "History",			szAppHistoryPath },
	{ "Select screens",		szAppSelectPath },
	{ "Versus screens",		szAppVersusPath },
	{ "How to play",		szAppHowtoPath },
	{ "Scores",				szAppScoresPath },
	{ "Bosses",				szAppBossesPath },
	{ "Game over",			szAppGameoverPath },
	{ "Flyers",				szAppFlyersPath },
	{ "Marquees",			szAppMarqueesPath },
	{ "Control panels",		szAppControlsPath },
	{ "Cabinets",			szAppCabinetsPath },
	{ "PCBs",				szAppPCBsPath },
	{ "Lists (lst)",		szAppListsPath },
	{ "Lists (dat)",		szAppDatListsPath },
	{ "Archives",			szAppArchivesPath },
};

#define SUPPORT_PATHS ((int)(sizeof(SupportPaths) / sizeof(SupportPaths[0])))

static char szEditSupport[SUPPORT_PATHS][MAX_PATH];

void UiRomPathsOpen()		{ bRomPathsRequest = true; }
void UiSupportPathsOpen()	{ bSupportPathsRequest = true; }

static void FixSlash(char* pszPath)
{
	int nLen = strlen(pszPath);
	if (nLen > 0 && pszPath[nLen - 1] != '/' && pszPath[nLen - 1] != '\\' && nLen + 1 < MAX_PATH) {
		pszPath[nLen] = '/';
		pszPath[nLen + 1] = '\0';
	}
}

// one line: the text box, a browse button and a clear button
static bool PathRow(int nRow, char* pszPath)
{
	bool bBrowse = false;

	ImGui::PushID(nRow);
	float fButtons = ImGui::GetFrameHeight() * 2.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f + ImGui::CalcTextSize("...").x;
	ImGui::SetNextItemWidth(-fButtons);
	ImGui::InputText("##path", pszPath, MAX_PATH);
	ImGui::SameLine();
	if (ImGui::Button("...")) {
		bBrowse = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("X", ImVec2(ImGui::GetFrameHeight(), 0.0f))) {
		pszPath[0] = '\0';
	}
	ImGui::PopID();

	return bBrowse;
}

// the browser is opened and drawn at the same level of the id stack
static void Browser(int nBrowse, char (*pszPaths)[MAX_PATH])
{
	if (nBrowse >= 0) {
		nBrowseRow = nBrowse;
		UiFileBrowserOpen("Choose a folder###browse", pszPaths[nBrowse], UI_BROWSE_FOLDER, NULL);
	}
	if (nBrowseRow >= 0 && UiFileBrowserDraw("Choose a folder###browse", pszPaths[nBrowseRow], MAX_PATH)) {
		nBrowseRow = -1;
	}
}

static bool CancelPressed(float fButton)
{
	return ImGui::Button("Cancel", ImVec2(fButton, 0.0f)) || UiEscapePressed();
}

static void DrawRomPaths()
{
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(UiDialogSize(640.0f, vp->WorkSize.y * 0.8f), ImGuiCond_Appearing);

	if (!ImGui::BeginPopupModal("ROM Paths###rompaths", NULL, ImGuiWindowFlags_NoSavedSettings)) {
		return;
	}

	ImGui::TextWrapped("Folders searched for the rom archives (zip, 7z), in this order.");
	ImGui::Spacing();

	float fBottom = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
	if (ImGui::BeginChild("##rows", ImVec2(0.0f, -fBottom))) {
		int nBrowse = -1;
		if (ImGui::BeginTable("##roms", 2, ImGuiTableFlags_SizingFixedFit)) {
			ImGui::TableSetupColumn("##n");
			ImGui::TableSetupColumn("##path", ImGuiTableColumnFlags_WidthStretch);
			for (int i = 0; i < DIRS_MAX; i++) {
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::AlignTextToFramePadding();
				ImGui::Text("%2d", i + 1);
				ImGui::TableNextColumn();
				if (PathRow(i, szEditRoms[i])) {
					nBrowse = i;
				}
			}
			ImGui::EndTable();
		}
		Browser(nBrowse, szEditRoms);
	}
	ImGui::EndChild();

	float fButton = 110.0f * fUiScale;
	if (ImGui::Button("OK", ImVec2(fButton, 0.0f))) {
		bool bChanged = false;
		for (int i = 0; i < DIRS_MAX; i++) {
			FixSlash(szEditRoms[i]);
			if (strcmp(szEditRoms[i], szAppRomPaths[i]) != 0) {
				strcpy(szAppRomPaths[i], szEditRoms[i]);
				bChanged = true;
			}
		}
		if (bChanged) {
			UiGameSelRescan();
		}
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (CancelPressed(fButton)) {
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

static void DrawSupportPaths()
{
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(UiDialogSize(700.0f, vp->WorkSize.y * 0.8f), ImGuiCond_Appearing);

	if (!ImGui::BeginPopupModal("Support Paths###supportpaths", NULL, ImGuiWindowFlags_NoSavedSettings)) {
		return;
	}

	float fBottom = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
	if (ImGui::BeginChild("##rows", ImVec2(0.0f, -fBottom))) {
		int nBrowse = -1;
		if (ImGui::BeginTable("##support", 2, ImGuiTableFlags_SizingFixedFit)) {
			ImGui::TableSetupColumn("##name");
			ImGui::TableSetupColumn("##path", ImGuiTableColumnFlags_WidthStretch);
			for (int i = 0; i < SUPPORT_PATHS; i++) {
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(SupportPaths[i].pszName);
				ImGui::TableNextColumn();
				if (PathRow(i, szEditSupport[i])) {
					nBrowse = i;
				}
			}
			ImGui::EndTable();
		}
		Browser(nBrowse, szEditSupport);
	}
	ImGui::EndChild();

	float fButton = 110.0f * fUiScale;
	if (ImGui::Button("OK", ImVec2(fButton, 0.0f))) {
		bool bImages = false;
		for (int i = 0; i < SUPPORT_PATHS; i++) {
			FixSlash(szEditSupport[i]);
			if (strcmp(szEditSupport[i], SupportPaths[i].pszPath) != 0) {
				strcpy(SupportPaths[i].pszPath, szEditSupport[i]);
				if (SupportPaths[i].pszPath == szAppPreviewsPath || SupportPaths[i].pszPath == szAppTitlesPath || SupportPaths[i].pszPath == szAppIconsPath) {
					bImages = true;
				}
			}
		}
		if (bImages) {
			UiGameSelReloadImages();
		}
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (CancelPressed(fButton)) {
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

void UiPathsDraw()
{
	if (bRomPathsRequest) {
		bRomPathsRequest = false;
		for (int i = 0; i < DIRS_MAX; i++) {
			snprintf(szEditRoms[i], MAX_PATH, "%s", szAppRomPaths[i]);
		}
		nBrowseRow = -1;
		ImGui::OpenPopup("ROM Paths###rompaths");
	}
	if (bSupportPathsRequest) {
		bSupportPathsRequest = false;
		for (int i = 0; i < SUPPORT_PATHS; i++) {
			snprintf(szEditSupport[i], MAX_PATH, "%s", SupportPaths[i].pszPath);
		}
		nBrowseRow = -1;
		ImGui::OpenPopup("Support Paths###supportpaths");
	}

	DrawRomPaths();
	DrawSupportPaths();
}
