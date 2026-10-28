// File and folder browser, drawn by the interface itself so it works in
// fullscreen and with a gamepad (no native dialogs)
#include "burner.h"
#include "ui.h"
#include "imgui.h"
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <vector>
#include <string>
#include <algorithm>

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

struct BrowserEntry {
	std::string Name;
	bool bFolder;
};

static char szDir[MAX_PATH] = "";			// absolute, with the trailing slash
static char szFileName[MAX_PATH] = "";
static std::vector<BrowserEntry> Entries;
static int nSelected = -1;
static bool bFolders = false;
static bool bRefresh = true;
static std::string Extensions;				// ".png;.zip" or empty for all

static bool IsSeparator(char c)
{
	return c == '/' || c == '\\';
}

static void AddSlash(char* pszPath, int nSize)
{
	int nLen = strlen(pszPath);
	if (nLen > 0 && !IsSeparator(pszPath[nLen - 1]) && nLen + 1 < nSize) {
		pszPath[nLen] = '/';
		pszPath[nLen + 1] = '\0';
	}
}

static void MakeAbsolute(const char* pszPath, char* pszOut, int nSize)
{
#ifdef _WIN32
	if (_fullpath(pszOut, (pszPath && pszPath[0]) ? pszPath : ".", nSize) == NULL) {
		snprintf(pszOut, nSize, "%s", pszPath);
	}
	for (char* p = pszOut; *p; p++) {
		if (*p == '\\') *p = '/';
	}
#else
	char szReal[PATH_MAX];
	if (realpath((pszPath && pszPath[0]) ? pszPath : ".", szReal)) {
		snprintf(pszOut, nSize, "%s", szReal);
	} else {
		snprintf(pszOut, nSize, "%s", pszPath);
	}
#endif
	AddSlash(pszOut, nSize);
}

static bool IsFolder(const char* pszPath)
{
	struct stat st;
	return stat(pszPath, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool HasExtension(const std::string& Name)
{
	if (Extensions.empty()) {
		return true;
	}

	size_t nDot = Name.rfind('.');
	if (nDot == std::string::npos) {
		return false;
	}
	std::string Ext = Name.substr(nDot);
	std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::tolower);

	size_t nStart = 0;
	while (nStart < Extensions.size()) {
		size_t nEnd = Extensions.find(';', nStart);
		if (nEnd == std::string::npos) nEnd = Extensions.size();
		if (Extensions.compare(nStart, nEnd - nStart, Ext) == 0) {
			return true;
		}
		nStart = nEnd + 1;
	}
	return false;
}

static void ReadFolder()
{
	bRefresh = false;
	Entries.clear();
	nSelected = -1;

	DIR* dir = opendir(szDir);
	if (dir == NULL) {
		return;
	}

	struct dirent* de;
	while ((de = readdir(dir)) != NULL) {
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
			continue;
		}
		char szFull[MAX_PATH * 2];
		snprintf(szFull, sizeof(szFull), "%s%s", szDir, de->d_name);

		BrowserEntry entry;
		entry.Name = de->d_name;
		entry.bFolder = IsFolder(szFull);
		if (!entry.bFolder && (bFolders || !HasExtension(entry.Name))) {
			continue;
		}
		Entries.push_back(entry);
	}
	closedir(dir);

	std::sort(Entries.begin(), Entries.end(), [](const BrowserEntry& a, const BrowserEntry& b) {
		if (a.bFolder != b.bFolder) {
			return a.bFolder;
		}
		return _stricmp(a.Name.c_str(), b.Name.c_str()) < 0;
	});
}

static void GoUp()
{
	int nLen = strlen(szDir);
	if (nLen > 0 && IsSeparator(szDir[nLen - 1])) {
		nLen--;
	}
	while (nLen > 0 && !IsSeparator(szDir[nLen - 1])) {
		nLen--;
	}
	if (nLen > 0) {
		szDir[nLen] = '\0';
		bRefresh = true;
	}
}

static void Enter(const char* pszName)
{
	int nLen = strlen(szDir);
	snprintf(szDir + nLen, sizeof(szDir) - nLen, "%s/", pszName);
	bRefresh = true;
}

// The path to give back: relative to the working directory when it's inside it
static void ResultPath(const char* pszPath, char* pszOut, int nSize)
{
	char szCwd[MAX_PATH];
	if (getcwd(szCwd, sizeof(szCwd))) {
		char szBase[MAX_PATH];
		MakeAbsolute(szCwd, szBase, sizeof(szBase));
		int nBase = strlen(szBase);
#ifdef _WIN32
		bool bInside = _strnicmp(pszPath, szBase, nBase) == 0;
#else
		bool bInside = strncmp(pszPath, szBase, nBase) == 0;
#endif
		if (bInside && pszPath[nBase]) {
			snprintf(pszOut, nSize, "%s", pszPath + nBase);
			return;
		}
	}
	snprintf(pszOut, nSize, "%s", pszPath);
}

void UiFileBrowserOpen(const char* pszPopup, const char* pszStart, bool bPickFolder, const char* pszExtensions)
{
	char szStart[MAX_PATH];
	snprintf(szStart, sizeof(szStart), "%s", pszStart ? pszStart : "");

	bFolders = bPickFolder;
	Extensions = pszExtensions ? pszExtensions : "";
	szFileName[0] = '\0';

	// start in the folder of the current value, or the nearest one that exists
	if (!bFolders && szStart[0] && !IsFolder(szStart)) {
		const char* pszName = szStart + strlen(szStart);
		while (pszName > szStart && !IsSeparator(pszName[-1])) pszName--;
		snprintf(szFileName, sizeof(szFileName), "%s", pszName);
	}
	MakeAbsolute(szStart, szDir, sizeof(szDir));
	while (!IsFolder(szDir) && strlen(szDir) > 3) {
		GoUp();
	}
	if (!IsFolder(szDir)) {
		MakeAbsolute(".", szDir, sizeof(szDir));
	}

	bRefresh = true;
	ImGui::OpenPopup(pszPopup);
}

bool UiFileBrowserDraw(const char* pszPopup, char* pszResult, int nSize)
{
	bool bDone = false;

	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x * 0.6f, vp->WorkSize.y * 0.7f), ImGuiCond_Appearing);

	if (!ImGui::BeginPopupModal(pszPopup, NULL, ImGuiWindowFlags_NoSavedSettings)) {
		return false;
	}

	if (bRefresh) {
		ReadFolder();
	}

	if (ImGui::Button("Up")) {
		GoUp();
	}
#ifdef _WIN32
	ImGui::SameLine();
	ImGui::SetNextItemWidth(70.0f * fUiScale);
	char szDrive[4] = { szDir[0], ':', 0, 0 };
	if (ImGui::BeginCombo("##drive", szDrive)) {
		DWORD nDrives = GetLogicalDrives();
		for (int i = 0; i < 26; i++) {
			if (nDrives & (1 << i)) {
				char szName[4] = { (char)('A' + i), ':', 0, 0 };
				if (ImGui::Selectable(szName, szDir[0] == 'A' + i)) {
					snprintf(szDir, sizeof(szDir), "%c:/", 'A' + i);
					bRefresh = true;
				}
			}
		}
		ImGui::EndCombo();
	}
#endif
	ImGui::SameLine();
	ImGui::TextUnformatted(UiText(szDir));

	float fBottom = ImGui::GetFrameHeightWithSpacing() * (bFolders ? 1.0f : 2.0f) + ImGui::GetStyle().ItemSpacing.y;
	if (ImGui::BeginChild("##entries", ImVec2(0.0f, -fBottom), ImGuiChildFlags_Borders)) {
		std::string OpenFolder;
		for (int i = 0; i < (int)Entries.size(); i++) {
			const BrowserEntry& entry = Entries[i];
			char szLabel[MAX_PATH + 8];
			snprintf(szLabel, sizeof(szLabel), "%s%s", entry.bFolder ? "[ ] " : "", UiText(entry.Name.c_str()));
			if (ImGui::Selectable(szLabel, nSelected == i, ImGuiSelectableFlags_AllowDoubleClick)) {
				nSelected = i;
				if (!entry.bFolder) {
					snprintf(szFileName, sizeof(szFileName), "%s", entry.Name.c_str());
				}
				// a double click, or Enter / the gamepad button, opens the folder or picks the file
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) || !ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
					if (entry.bFolder) {
						OpenFolder = entry.Name;
					} else {
						bDone = true;
					}
				}
			}
		}
		if (!OpenFolder.empty()) {
			Enter(OpenFolder.c_str());
		}
	}
	ImGui::EndChild();

	if (!bFolders) {
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##file", "File name", szFileName, sizeof(szFileName));
	}

	float fButton = 110.0f * fUiScale;
	if (ImGui::Button(bFolders ? "Use This Folder" : "Open", ImVec2(fButton * (bFolders ? 1.4f : 1.0f), 0.0f)) && (bFolders || szFileName[0])) {
		bDone = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(fButton, 0.0f)) || UiEscapePressed()) {
		ImGui::CloseCurrentPopup();
	}

	if (bDone) {
		char szPath[MAX_PATH * 2];
		if (bFolders) {
			// a selected folder that wasn't opened counts too
			if (nSelected >= 0 && Entries[nSelected].bFolder) {
				snprintf(szPath, sizeof(szPath), "%s%s/", szDir, Entries[nSelected].Name.c_str());
			} else {
				snprintf(szPath, sizeof(szPath), "%s", szDir);
			}
		} else {
			snprintf(szPath, sizeof(szPath), "%s%s", szDir, szFileName);
		}
		ResultPath(szPath, pszResult, nSize);
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();

	return bDone;
}
