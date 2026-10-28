// Game > Replay (win32/replay.cpp dialogs: record and play back the inputs,
// .fr) and Audio > Record Sound (win32/wave.cpp), with the frame counter and
// the joysticks of the playback over the game. The engines are ../replay.cpp
// and ../wave.cpp.
#include "burner.h"
#include "gamelist_model.h"
#include "ui.h"
#include "imgui.h"
#include <dirent.h>
#include <vector>
#include <string>
#include <algorithm>

enum { REQUEST_NONE, REQUEST_RECORD, REQUEST_PLAYBACK, REQUEST_WAVE };
static int nRequest = REQUEST_NONE;

// the record dialog
static char szRecordFile[MAX_PATH] = "";
static char szRecordAuthor[512] = "";
static bool bRecordPowerOn = false;
static bool bRecordNvram = true;

// the playback dialog
static std::vector<std::string> Recordings;
static char szPlayFile[MAX_PATH] = "";
static ReplayInfo PlayInfo;
static bool bPlayReadOnly = true;
static bool bPlayStartPaused = false;

// what waits for the game to be (re)started
enum { PENDING_NONE, PENDING_RECORD, PENDING_PLAYBACK };
static int nPending = PENDING_NONE;
static int nPendingDrv = -1;

static const char* RecordingsFolder()
{
	static char szFolder[MAX_PATH] = "";
	if (szFolder[0] == '\0') {
#if !defined(SDL_WINDOWS)
		char* pszPath = SDL_GetPrefPath("fbneo", "recordings");
		snprintf(szFolder, sizeof(szFolder), "%s", pszPath ? pszPath : "");
		SDL_free(pszPath);
#else
		snprintf(szFolder, sizeof(szFolder), "recordings/");
		MakeDirs(szFolder);
#endif
	}
	return szFolder;
}

static bool FileExists(const char* pszFile)
{
	FILE* fp = fopen(pszFile, "rb");
	if (fp) {
		fclose(fp);
		return true;
	}
	return false;
}

// recordings/<game>.fr, <game>-1.fr... the first one free
static void UniqueName(char* pszFile, int nSize, const char* pszExt)
{
	const char* pszGame = BurnDrvGetTextA(DRV_NAME);
	snprintf(pszFile, nSize, "%s%s%s", RecordingsFolder(), pszGame, pszExt);
	for (int i = 1; FileExists(pszFile) && i < 1000; i++) {
		snprintf(pszFile, nSize, "%s%s-%d%s", RecordingsFolder(), pszGame, i, pszExt);
	}
}

static void Error(const char* pszText)
{
	AppError((TCHAR*)pszText, 0);
}

// ----------------------------------------------------------------------------
// The menus

void UiRecordOpen(int nWhat)
{
	nRequest = (nWhat == 0) ? REQUEST_RECORD : (nWhat == 1) ? REQUEST_PLAYBACK : REQUEST_WAVE;
}

void UiReplayMenu()
{
	bool bRunning = UiGameRunning();

	if (ImGui::MenuItem("Record Input...", NULL, false, bRunning && nReplayStatus == 0)) {
		UiRecordOpen(0);
	}
	if (ImGui::MenuItem("Play Back Input...", NULL, false, nReplayStatus == 0)) {
		UiRecordOpen(1);
	}
	if (ImGui::MenuItem(nReplayStatus == 1 ? "Stop Recording" : "Stop Playback", NULL, false, nReplayStatus != 0)) {
		bool bRecording = nReplayStatus == 1;
		StopReplay();
		UpdateMessage((char*)(bRecording ? "Recording stopped" : "Playback stopped"));
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Read-Only Playback", NULL, bReplayReadOnly)) {
		bReplayReadOnly = !bReplayReadOnly;		// loading a state then continues the playback or records from there
	}
	if (ImGui::MenuItem("Show the Frame Counter", NULL, bReplayFrameCounterDisplay)) {
		bReplayFrameCounterDisplay = !bReplayFrameCounterDisplay;
	}
	if (ImGui::MenuItem("Show the Joysticks of the Playback", NULL, bReplayShowMovement)) {
		bReplayShowMovement = !bReplayShowMovement;
	}
}

void UiWaveMenuItems()
{
	if (WaveLog == NULL) {
		if (ImGui::MenuItem("Record Sound (WAV)...", NULL, false, UiGameRunning())) {
			UiRecordOpen(2);
		}
	} else if (ImGui::MenuItem("Stop Recording Sound")) {
		WaveLogStop();
		UpdateMessage((char*)"Sound recording stopped");
	}
}

// ----------------------------------------------------------------------------
// The starts, once the game runs (from power-on: restarted)

static void Restart(int nDrv, bool bNvram)
{
	nRunInitAutoState = bNvram ? 1 : 0;		// RunInit(): no automatic state, or the NVRAM only
	nPendingDrv = nDrv;
	UiLaunchGame(nDrv);
}

static void DoRecord()
{
	if (StartRecord(szRecordFile, szRecordAuthor, bRecordPowerOn, bRecordPowerOn && bRecordNvram) == 0) {
		UpdateMessage((char*)"Recording started");
	}
}

static void DoPlayback()
{
	bReplayReadOnly = bPlayReadOnly;
	if (StartReplay(szPlayFile, bPlayStartPaused) == 0) {
		UpdateMessage((char*)"Playback started");
	}
}

void UiRecordGameStarted()
{
	int nWhat = nPending;
	nPending = PENDING_NONE;
	if (nWhat == PENDING_NONE || (UINT32)nPendingDrv != nBurnDrvActive) {
		return;
	}
	if (nWhat == PENDING_RECORD) {
		DoRecord();
	} else {
		DoPlayback();
	}
}

void UiRecordGameStopping()
{
	StopReplay();
	WaveLogStop();
}

// ----------------------------------------------------------------------------
// Record dialog

static void RecordDialog()
{
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (!ImGui::BeginPopupModal("Record Input###recordinput", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
		return;
	}

	float fWidth = 480.0f * fUiScale;
	ImGui::TextUnformatted("File");
	ImGui::SetNextItemWidth(fWidth - ImGui::CalcTextSize("...").x - ImGui::GetStyle().FramePadding.x * 2.0f - ImGui::GetStyle().ItemSpacing.x);
	ImGui::InputText("##file", szRecordFile, sizeof(szRecordFile));
	ImGui::SameLine();
	if (ImGui::Button("...")) {
		UiFileBrowserOpen("Record Input To###recbrowse", szRecordFile, UI_BROWSE_SAVE, ".fr");
	}
	char szPicked[MAX_PATH];
	if (UiFileBrowserDraw("Record Input To###recbrowse", szPicked, sizeof(szPicked))) {
		snprintf(szRecordFile, sizeof(szRecordFile), "%s", szPicked);
	}

	ImGui::TextUnformatted("Author, comments");
	ImGui::InputTextMultiline("##author", szRecordAuthor, sizeof(szRecordAuthor), ImVec2(fWidth, ImGui::GetTextLineHeight() * 4.0f));

	ImGui::Checkbox("Record from power-on (the game is restarted)", &bRecordPowerOn);
	ImGui::BeginDisabled(!bRecordPowerOn);
	ImGui::Indent();
	ImGui::Checkbox("With the NVRAM of the game", &bRecordNvram);
	ImGui::Unindent();
	ImGui::EndDisabled();
	if (!bRecordPowerOn) {
		ImGui::TextDisabled("The recording starts from the current state of the game.");
	}

	bool bExists = szRecordFile[0] && FileExists(szRecordFile);
	if (bExists) {
		ImGui::TextDisabled("The file already exists and will be replaced.");
	}

	ImGui::Spacing();
	float fButton = 120.0f * fUiScale;
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + fWidth - fButton * 2.0f - ImGui::GetStyle().ItemSpacing.x);
	ImGui::BeginDisabled(szRecordFile[0] == '\0');
	if (ImGui::Button("Record", ImVec2(fButton, 0.0f))) {
		if (bRecordPowerOn) {
			nPending = PENDING_RECORD;
			Restart(nBurnDrvActive, bRecordNvram);
		} else {
			DoRecord();
		}
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(fButton, 0.0f)) || UiEscapePressed()) {
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

// ----------------------------------------------------------------------------
// Playback dialog

static bool IsRecording(const char* pszName)
{
	int nLen = strlen(pszName);
	return nLen > 3 && _stricmp(pszName + nLen - 3, ".fr") == 0;
}

// the recordings of the running game, or all of them
static void ListRecordings()
{
	Recordings.clear();
	const char* pszGame = UiGameRunning() ? BurnDrvGetTextA(DRV_NAME) : NULL;
	DIR* d = opendir(RecordingsFolder());
	if (d) {
		struct dirent* ent;
		while ((ent = readdir(d)) != NULL) {
			if (IsRecording(ent->d_name) && (pszGame == NULL || _strnicmp(ent->d_name, pszGame, strlen(pszGame)) == 0)) {
				Recordings.push_back(std::string(RecordingsFolder()) + ent->d_name);
			}
		}
		closedir(d);
	}
	std::sort(Recordings.begin(), Recordings.end());
}

static void SelectRecording(const char* pszFile)
{
	snprintf(szPlayFile, sizeof(szPlayFile), "%s", pszFile);
	ReplayGetInfo(szPlayFile, &PlayInfo);
	if (PlayInfo.bReadOnlyFile) {
		bPlayReadOnly = true;
	}
}

static void StartPlayback()
{
	int nDrv = GameListFind(PlayInfo.szGame);
	if (nDrv < 0) {
		char szText[256];
		snprintf(szText, sizeof(szText), "This recording is for %s, which this version doesn't have.\n", PlayInfo.szGame);
		Error(szText);
		return;
	}

	if (PlayInfo.bFromPowerOn) {
		nPending = PENDING_PLAYBACK;
		Restart(nDrv, PlayInfo.bWithNvram);
	} else if (UiGameRunning() && (UINT32)nDrv == nBurnDrvActive) {
		DoPlayback();							// the state of the recording is loaded in the running game
	} else {
		nPending = PENDING_PLAYBACK;
		nPendingDrv = nDrv;
		UiLaunchGame(nDrv);
	}
}

static void PlaybackDialog()
{
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (!ImGui::BeginPopupModal("Play Back Input###playbackinput", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
		return;
	}

	float fWidth = 520.0f * fUiScale;
	ImGui::TextUnformatted("Recording");
	ImGui::SetNextItemWidth(fWidth - ImGui::CalcTextSize("...").x - ImGui::GetStyle().FramePadding.x * 2.0f - ImGui::GetStyle().ItemSpacing.x);
	const char* pszShown = szPlayFile[0] ? szPlayFile : "(none)";
	if (ImGui::BeginCombo("##file", UiText(pszShown))) {
		for (size_t i = 0; i < Recordings.size(); i++) {
			if (ImGui::Selectable(UiText(Recordings[i].c_str()), Recordings[i] == szPlayFile)) {
				SelectRecording(Recordings[i].c_str());
			}
		}
		if (Recordings.empty()) {
			ImGui::TextDisabled("No recordings in %s", RecordingsFolder());
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::Button("...")) {
		UiFileBrowserOpen("Play Back Input From###playbrowse", szPlayFile[0] ? szPlayFile : RecordingsFolder(), UI_BROWSE_OPEN, ".fr");
	}
	char szPicked[MAX_PATH];
	if (UiFileBrowserDraw("Play Back Input From###playbrowse", szPicked, sizeof(szPicked))) {
		SelectRecording(szPicked);
	}

	ImGui::Spacing();
	if (szPlayFile[0] && !PlayInfo.bValid) {
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s", PlayInfo.szError);
	} else if (PlayInfo.bValid && ImGui::BeginTable("##props", 2, ImGuiTableFlags_SizingFixedFit)) {
		ImGui::TableSetupColumn("##label");
		ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthFixed, fWidth * 0.7f);

		int nFps = nBurnFPS > 0 ? nBurnFPS : 6000;
		int nSeconds = (int)(((INT64)PlayInfo.nFrames * 100 + (nFps >> 1)) / nFps);
		char szValue[128];
		const char* pszLabels[] = { "Game:", "Length:", "Frames:", "Undo count:", "Recorded from:", "Recorded on:", "Author:" };
		for (int i = 0; i < 7; i++) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextDisabled("%s", pszLabels[i]);
			ImGui::TableNextColumn();
			switch (i) {
				case 0: {
					int nDrv = GameListFind(PlayInfo.szGame);
					ImGui::TextWrapped("%s", nDrv >= 0 ? UiText(GameListGet(nDrv)->pszFullName) : PlayInfo.szGame);
					break;
				}
				case 1: ImGui::Text("%02d:%02d:%02d", nSeconds / 3600, (nSeconds / 60) % 60, nSeconds % 60); break;
				case 2: ImGui::Text("%u", PlayInfo.nFrames); break;
				case 3: ImGui::Text("%u", PlayInfo.nUndoCount); break;
				case 4: {
					UINT32 v = PlayInfo.nFBVersion;
					snprintf(szValue, sizeof(szValue), "%s%s", PlayInfo.bFromPowerOn ? "Power-on" : "Savestate", PlayInfo.bWithNvram ? " w/NVRAM" : "");
					if (v) {
						ImGui::Text("%s, v%x.%x.%x.%02x", szValue, v >> 20, (v >> 16) & 0x0F, (v >> 8) & 0xFF, v & 0xFF);
					} else {
						ImGui::TextUnformatted(szValue);
					}
					break;
				}
				case 5: ImGui::TextUnformatted(PlayInfo.szDate); break;
				case 6: ImGui::TextWrapped("%s", PlayInfo.szAuthor); break;
			}
		}
		ImGui::EndTable();

		// the CD games need their image, which the recording doesn't have
		if (!strncmp(PlayInfo.szGame, "neocdz", 6) || !strncmp(PlayInfo.szGame, "pce_scdsys", 10)) {
			ImGui::TextDisabled("A CD game: the CD image of the recording has to be loaded (Game > Quick Load).");
		}
	}

	ImGui::Spacing();
	ImGui::BeginDisabled(PlayInfo.bReadOnlyFile);
	ImGui::Checkbox("Read-only (loading a state goes on with the playback)", &bPlayReadOnly);
	ImGui::EndDisabled();
	ImGui::Checkbox("Show the joysticks", &bReplayShowMovement);
	ImGui::Checkbox("Start paused", &bPlayStartPaused);

	ImGui::Spacing();
	float fButton = 120.0f * fUiScale;
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + fWidth - fButton * 2.0f - ImGui::GetStyle().ItemSpacing.x);
	ImGui::BeginDisabled(!PlayInfo.bValid);
	if (ImGui::Button("Play", ImVec2(fButton, 0.0f))) {
		StartPlayback();
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(fButton, 0.0f)) || UiEscapePressed()) {
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

// ----------------------------------------------------------------------------

void UiRecordDraw()
{
	char szPicked[MAX_PATH];

	switch (nRequest) {
		case REQUEST_RECORD:
			UniqueName(szRecordFile, sizeof(szRecordFile), ".fr");
			ImGui::OpenPopup("Record Input###recordinput");
			break;
		case REQUEST_PLAYBACK:
			ListRecordings();
			memset(&PlayInfo, 0, sizeof(PlayInfo));
			szPlayFile[0] = '\0';
			if (!Recordings.empty()) {
				SelectRecording(Recordings.back().c_str());
			}
			ImGui::OpenPopup("Play Back Input###playbackinput");
			break;
		case REQUEST_WAVE:
			UniqueName(szPicked, sizeof(szPicked), ".wav");
			UiFileBrowserOpen("Record Sound To###wavbrowse", szPicked, UI_BROWSE_SAVE, ".wav");
			break;
	}
	nRequest = REQUEST_NONE;

	RecordDialog();
	PlaybackDialog();

	if (UiFileBrowserDraw("Record Sound To###wavbrowse", szPicked, sizeof(szPicked))) {
		if (!UiGameRunning()) {
			return;
		}
		if (WaveLogStart(szPicked)) {
			Error("The sound file couldn't be created.\n");
		} else {
			UpdateMessage((char*)"Recording sound");
		}
	}
}

// ----------------------------------------------------------------------------
// Over the game: "REC", the frame counter and the joysticks of the playback

const char* UiRecordStatus()
{
	static char szText[64];
	UINT32 nFrame = GetCurrentFrame() - nStartFrame;

	if (nReplayStatus == 1) {
		if (bReplayFrameCounterDisplay) snprintf(szText, sizeof(szText), "REC %u", nFrame);
		else snprintf(szText, sizeof(szText), "REC");
	} else if (nReplayStatus == 2) {
		if (bReplayFrameCounterDisplay) snprintf(szText, sizeof(szText), "PLAY %u / %u", nFrame, nTotalFrames);
		else snprintf(szText, sizeof(szText), "PLAY");
	} else {
		szText[0] = '\0';
	}
	if (WaveLog) {
		int nLen = strlen(szText);
		snprintf(szText + nLen, sizeof(szText) - nLen, "%sWAV", nLen ? "  " : "");
	}
	return szText[0] ? szText : NULL;
}

// PrintInputs() of win32/replay.cpp: the directions and the fire buttons
// of players 1 and 2, shown for 5 seconds after they were used
void UiRecordDrawMovement(ImDrawList* dl, ImVec2 pos)
{
	static UINT32 nActive[2] = { 0, 0 };
	static UINT32 nLastFrame = 0;

	if (nReplayStatus != 2 || !bReplayShowMovement) {
		return;
	}

	UINT32 nFrame = GetCurrentFrame();
	if (nFrame < nLastFrame) {
		nActive[0] = nActive[1] = 0;			// a state was loaded
	}
	nLastFrame = nFrame;

	bool UDLR[2][4];
	char Buttons[2][6];
	memset(UDLR, 0, sizeof(UDLR));
	memset(Buttons, 0, sizeof(Buttons));

	struct BurnInputInfo bii;
	for (UINT32 i = 0; i < nGameInpCount; i++) {
		memset(&bii, 0, sizeof(bii));
		BurnDrvGetInputInfo(&bii, i);
		if (!bii.pVal || !bii.szInfo || bii.szInfo[0] != 'p' || (bii.szInfo[1] != '1' && bii.szInfo[1] != '2')) {
			continue;
		}
		int p = bii.szInfo[1] - '1';
		const char* pszWhat = bii.szInfo + 2;
		if (bii.nType & BIT_GROUP_ANALOG) {
			INT16 v = bii.pShortVal ? (INT16)*bii.pShortVal : 0;
			if (_stricmp(pszWhat, " x-axis") == 0 && bii.nType == BIT_GROUP_ANALOG) {
				if (v > 0x80) { UDLR[p][3] = true; nActive[p] = nFrame; }
				if (v < -0x80) { UDLR[p][2] = true; nActive[p] = nFrame; }
			}
			if (_stricmp(pszWhat, " y-axis") == 0 && bii.nType == BIT_GROUP_ANALOG) {
				if (v > 0x80) { UDLR[p][1] = true; nActive[p] = nFrame; }
				if (v < -0x80) { UDLR[p][0] = true; nActive[p] = nFrame; }
			}
			continue;
		}
		if (*bii.pVal == 0) {
			continue;
		}
		static const char* pszDirs[4] = { " Up", " Down", " Left", " Right" };
		for (int d = 0; d < 4; d++) {
			if (_stricmp(pszWhat, pszDirs[d]) == 0) {
				UDLR[p][d] = true;
				nActive[p] = nFrame;
			}
		}
		if (_strnicmp(pszWhat, " fire ", 6) == 0 && pszWhat[6] >= '1' && pszWhat[6] <= '6') {
			Buttons[p][pszWhat[6] - '1'] = pszWhat[6];
			nActive[p] = nFrame;
		}
	}

	float fLine = ImGui::GetTextLineHeight();
	for (int p = 0; p < 2; p++) {
		if (nActive[p] == 0 || nFrame > nActive[p] + 60 * 5) {
			continue;
		}
		char szLines[3][16];
		snprintf(szLines[0], sizeof(szLines[0]), "  %c   %c%c%c", UDLR[p][0] ? '^' : '.', Buttons[p][0] ? Buttons[p][0] : '.', Buttons[p][1] ? Buttons[p][1] : '.', Buttons[p][2] ? Buttons[p][2] : '.');
		snprintf(szLines[1], sizeof(szLines[1]), "%c   %c", UDLR[p][2] ? '<' : '.', UDLR[p][3] ? '>' : '.');
		snprintf(szLines[2], sizeof(szLines[2]), "  %c   %c%c%c", UDLR[p][1] ? 'v' : '.', Buttons[p][3] ? Buttons[p][3] : '.', Buttons[p][4] ? Buttons[p][4] : '.', Buttons[p][5] ? Buttons[p][5] : '.');
		ImVec2 at(pos.x + p * ImGui::CalcTextSize("P1  ^   123      ").x, pos.y - fLine * 4.0f);
		char szPlayer[4] = { 'P', (char)('1' + p), 0, 0 };
		dl->AddText(ImVec2(at.x + 1, at.y + 1), IM_COL32(0, 0, 0, 255), szPlayer);
		dl->AddText(at, IM_COL32(255, 240, 0, 255), szPlayer);
		for (int l = 0; l < 3; l++) {
			ImVec2 t(at.x, at.y + fLine * (l + 1));
			dl->AddText(ImVec2(t.x + 1, t.y + 1), IM_COL32(0, 0, 0, 255), szLines[l]);
			dl->AddText(t, IM_COL32(255, 255, 255, 255), szLines[l]);
		}
	}
}
