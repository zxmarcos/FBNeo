// Input mapping (win32/inpd.cpp, inps.cpp and inpc.cpp), DIP switches and
// cheats, drawn with ImGui. The game is paused while they are open (see
// UpdateState() in ui_app.cpp).
#include "burner.h"
#include "ui.h"
#include "imgui.h"
#include <dirent.h>
#include <vector>
#include <string>
#include <algorithm>
#ifdef _WIN32
#include <direct.h>
#endif

static bool bMappingRequest = false;
static bool bDipsRequest = false;
static bool bCheatsRequest = false;

void UiInputMappingOpen()	{ if (bDrvOkay) bMappingRequest = true; }
void UiDipsOpen()			{ if (bDrvOkay) bDipsRequest = true; }
void UiCheatsOpen()			{ if (bDrvOkay) bCheatsRequest = true; }

// ----------------------------------------------------------------------------
// Capture of a control for one input (inps.cpp)

static int nCaptureInput = -1;				// the GameInp being mapped
static int nCaptureState = 0;				// InputFind() state: 4, 8, 16; | 0x100 for the second key of "map digital"
static int nCaptureCode = -1;
static int nCaptureFirst = -1;				// first key of "map digital"
static int nCaptureFrames = 0;
static bool bClearLock = false;
static bool bMapDigital = false;
static bool bGrabMouse = false;
static int nDigitalSpeed = 0x800;
static bool bOldLeftAltkeyMapped = false;
static struct BurnInputInfo CaptureInfo;

bool UiInputCapturing()
{
	return nCaptureInput >= 0;
}

static void CaptureStart(int nInput)
{
	memset(&CaptureInfo, 0, sizeof(CaptureInfo));
	BurnDrvGetInputInfo(&CaptureInfo, nInput);

	nCaptureInput = nInput;
	nCaptureCode = -1;
	nCaptureFirst = -1;
	nCaptureFrames = 0;
	bMapDigital = false;
	bGrabMouse = false;

	bOldLeftAltkeyMapped = bLeftAltkeyMapped;
	bLeftAltkeyMapped = true;				// so the left Alt key can be mapped

	InputFind(2);
	nCaptureState = 4;
}

static void CaptureEnd()
{
	bLeftAltkeyMapped = bOldLeftAltkeyMapped;
	nCaptureInput = -1;
	nCaptureState = 0;
	GameInpCheckLeftAlt();
	bSaveInputs = true;						// keep the changes when the game ends
}

static bool IsAnalogInput()
{
	return nCaptureInput < (int)nGameInpCount && (CaptureInfo.nType & BIT_GROUP_ANALOG);
}

// SetInput() of inps.cpp: returns 1 while it waits for the second key of "map digital"
static int SetInput(int nCode)
{
	struct GameInp* pgi = GameInp + nCaptureInput;

	if (nCaptureState & 0x100) {
		pgi->nInput = GIT_KEYSLIDER;
		pgi->Input.Slider.nSliderSpeed = nDigitalSpeed;
		if (nDigitalSpeed <= 0x1000) {
			pgi->Input.Slider.nSliderCenter = (0x1100 - nDigitalSpeed) / 0xdf;
		} else if (nDigitalSpeed <= 0x1800) {
			pgi->Input.Slider.nSliderCenter = 2;
		} else {
			pgi->Input.Slider.nSliderCenter = 1;
		}
		pgi->Input.Slider.SliderAxis.nSlider[0] = nCaptureFirst;
		pgi->Input.Slider.SliderAxis.nSlider[1] = nCode;
		return 0;
	}

	if (IsAnalogInput() && bMapDigital && nCode != 0) {
		nCaptureFirst = nCode;				// map an analog control to two keys: the first one
		return 1;
	}

	if ((pgi->nInput & GIT_GROUP_MACRO) == 0) {
		if (CaptureInfo.nType & BIT_GROUP_CONSTANT) {
			return 0;
		}

		if ((CaptureInfo.nType & BIT_GROUP_ANALOG) && (nCode & 0xFF) < 0x10) {
			if (CaptureInfo.szInfo && (strcmp(CaptureInfo.szInfo + 4, "-axis-neg") == 0 || strcmp(CaptureInfo.szInfo + 4, "-axis-pos") == 0)) {
				if ((nCode & 0xF000) == 0x4000) {
					pgi->nInput = (nCode & 1) ? GIT_JOYAXIS_POS : GIT_JOYAXIS_NEG;
					pgi->Input.JoyAxis.nJoy = (nCode & 0x0F00) >> 8;
					pgi->Input.JoyAxis.nAxis = (nCode & 0x0F) >> 1;
				}
			} else {						// the whole axis
				if ((nCode & 0xF000) == 0x4000) {
					pgi->nInput = GIT_JOYAXIS_FULL;
					pgi->Input.JoyAxis.nJoy = (nCode & 0x0F00) >> 8;
					pgi->Input.JoyAxis.nAxis = (nCode & 0x0F) >> 1;
				} else {
					pgi->nInput = GIT_MOUSEAXIS;
					pgi->Input.MouseAxis.nMouse = (nCode & 0x0F00) >> 8;
					pgi->Input.MouseAxis.nAxis = (nCode & 0x0F) >> 1;
				}
			}

			if (nCode == 0) {				// Clear Input
				pgi->nInput = 0;
				pgi->Input.JoyAxis.nJoy = 0;
				pgi->Input.JoyAxis.nAxis = 0;
				pgi->Input.MouseAxis.nMouse = 0;
				pgi->Input.MouseAxis.nAxis = 0;
				if (bClearLock) {			// a switch with code 0: no default or preset fills it again
					pgi->nInput = GIT_SWITCH;
					pgi->Input.Switch.nCode = 0;
				}
			}
		} else {
			pgi->nInput = GIT_SWITCH;
			if (nCode == 0 && !bClearLock) pgi->nInput = 0;
			pgi->Input.Switch.nCode = (UINT16)nCode;
		}
	} else {
		pgi->Macro.nMode = 0x01;			// the macro is in use
		if (nCode == 0) {
			if (!bClearLock) pgi->Macro.nMode = 0;
			if (pgi->Macro.nSysMacro == 15) pgi->Macro.nSysMacro = 0;	// no auto-fire without a control
		}
		pgi->Macro.Switch.nCode = (UINT16)nCode;
	}

	return 0;
}

// InpsUpdate() of inps.cpp, once per frame while the popup is open
static void CaptureUpdate()
{
	nCaptureFrames++;
	InputMake(false);						// polls the devices (the game may be paused)

	int nFind = InputFind(nCaptureState & 0xFF);

	if (nCaptureState & 4) {				// waiting for all the controls to be released
		if (bGrabMouse ? nFind >= 0 : (nFind >= 0 && nFind < 0x8000)) {
			return;
		}
		nCaptureState = (nCaptureState & 0xFF00) | 8;
		return;
	}

	if (nCaptureState & 8) {				// waiting for a control
		if (bGrabMouse ? nFind < 0 : (nFind < 0 || (nFind >= 0x8000 && nFind < 0xC000))) {
			return;
		}
		nCaptureCode = nFind;
		nCaptureState = (nCaptureState & 0xFF00) | 16;
		return;
	}

	if (nCaptureState & 16) {				// waiting for the control to be released
		if (bGrabMouse ? nFind >= 0 : (nFind >= 0 && nFind < 0x8000)) {
			if (nFind != nCaptureCode) {
				nCaptureCode = nFind;		// an axis that moves on
			}
			return;
		}

		if (SetInput(nCaptureCode) == 0) {
			CaptureEnd();
			return;
		}
		nCaptureState = 0x100 | 4;			// now the second key of "map digital"
	}
}

static const char* CaptureName()
{
	if (nCaptureInput >= (int)nGameInpCount) {
		return GameInp[nCaptureInput].Macro.szName;
	}
	return CaptureInfo.szName ? CaptureInfo.szName : "";
}

static void DrawCapture()
{
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (!ImGui::BeginPopupModal("Map Input###capture", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
		return;
	}

	if (nCaptureInput < 0) {
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}

	CaptureUpdate();
	if (nCaptureInput < 0) {				// done
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}

	ImGui::Text("%s", UiText(CaptureName()));
	ImGui::Separator();

	const char* pszText;
	if (nCaptureState & 4) {
		pszText = nCaptureFrames > 60 && nCaptureCode < 0 ? "Release all the controls..." : "Waiting...";
	} else if (nCaptureState & 8) {
		pszText = (nCaptureState & 0x100) ? "Press the second key (the other direction)" : "Press a key, a button, or move an axis";
	} else {
		pszText = InputCodeDesc(nCaptureCode);
	}
	ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s", pszText);
	if (nCaptureState & 0x100) {
		ImGui::TextDisabled("First key: %s", InputCodeDesc(nCaptureFirst));
	}
	ImGui::Spacing();

	ImGui::Checkbox("Clear Lock (a cleared input stays empty)", &bClearLock);
	if (IsAnalogInput()) {
		if (ImGui::Checkbox("Map Digital (two keys for the axis)", &bMapDigital)) {
			nCaptureState = 4;
		}
		if (bMapDigital) {
			ImGui::SetNextItemWidth(250.0f * fUiScale);
			ImGui::SliderInt("Speed", &nDigitalSpeed, 0x700, 0x2000, "%d");
		}
	}
	if (ImGui::Checkbox("Grab Mouse (map the mouse)", &bGrabMouse)) {
		InputFind(2);						// start again, the click on the checkbox isn't the control
		nCaptureState = (nCaptureState & 0xFF00) | 4;
	}

	ImGui::Spacing();
	float fButton = 120.0f * fUiScale;
	if (ImGui::Button("Clear Input", ImVec2(fButton, 0.0f))) {
		SetInput(0);
		CaptureEnd();
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(fButton, 0.0f))) {
		CaptureEnd();
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

// ----------------------------------------------------------------------------
// Constants (inpc.cpp): the value of a constant input, DIP switch blocks

static int nConstInput = -1;
static int nConstValue = 0;

static void DrawConstant()
{
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (!ImGui::BeginPopupModal("Constant###constant", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
		return;
	}

	struct BurnInputInfo bii;
	memset(&bii, 0, sizeof(bii));
	BurnDrvGetInputInfo(&bii, nConstInput);
	ImGui::Text("%s", UiText(bii.szName ? bii.szName : ""));
	ImGui::SetNextItemWidth(160.0f * fUiScale);
	ImGui::InputInt("Value (hex)", &nConstValue, 1, 16, ImGuiInputTextFlags_CharsHexadecimal);
	if (nConstValue < 0) nConstValue = 0;
	if (nConstValue > 0xFF) nConstValue = 0xFF;

	float fButton = 110.0f * fUiScale;
	if (ImGui::Button("OK", ImVec2(fButton, 0.0f))) {
		GameInp[nConstInput].nInput = GIT_CONSTANT;
		GameInp[nConstInput].Input.Constant.nConst = (UINT8)nConstValue;
		bSaveInputs = true;
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(fButton, 0.0f)) || UiEscapePressed()) {
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

// ----------------------------------------------------------------------------
// Presets (inpd.cpp)

static int nPresetPlayer = 0;
static int nPresetDevice = 0;				// 0 keyboard, 1-3 joysticks, 4-7 X-Arcade/HotRod, 8+ preset files
static int nPresetAnalog = 1;
static std::vector<std::string> PresetFiles;

static const char* pszDevices[] = {
	"Keyboard", "Joystick 1", "Joystick 2", "Joystick 3",
	"X-Arcade (left side)", "X-Arcade (right side)", "HotRod (left side)", "HotRod (right side)",
};

static void ReadPresetFiles()
{
	PresetFiles.clear();
	DIR* dir = opendir("config/presets");
	if (dir == NULL) {
		return;
	}
	struct dirent* de;
	while ((de = readdir(dir)) != NULL) {
		int nLen = strlen(de->d_name);
		if (nLen > 4 && _stricmp(de->d_name + nLen - 4, ".ini") == 0) {
			PresetFiles.push_back(std::string(de->d_name, nLen - 4));
		}
	}
	closedir(dir);
	std::sort(PresetFiles.begin(), PresetFiles.end());
}

static void GameInpConfigOne(int nPlayer, int nPcDev, int nAnalog, struct GameInp* pgi, char* szi)
{
	switch (nPcDev) {
		case 0:
			GamcPlayer(pgi, szi, nPlayer, -1);						// Keyboard
			GamcAnalogKey(pgi, szi, nPlayer, nAnalog);
			GamcMisc(pgi, szi, nPlayer);
			break;
		case 1:
		case 2:
		case 3:
			GamcPlayer(pgi, szi, nPlayer, nPcDev - 1);				// Joysticks
			GamcAnalogJoy(pgi, szi, nPlayer, nPcDev - 1, nAnalog);
			GamcMisc(pgi, szi, nPlayer);
			break;
		case 4: GamcPlayerHotRod(pgi, szi, nPlayer, 0x10, nAnalog); GamcMisc(pgi, szi, -1); break;	// X-Arcade left
		case 5: GamcPlayerHotRod(pgi, szi, nPlayer, 0x11, nAnalog); GamcMisc(pgi, szi, -1); break;	// X-Arcade right
		case 6: GamcPlayerHotRod(pgi, szi, nPlayer, 0x00, nAnalog); GamcMisc(pgi, szi, -1); break;	// HotRod left
		case 7: GamcPlayerHotRod(pgi, szi, nPlayer, 0x01, nAnalog); GamcMisc(pgi, szi, -1); break;	// HotRod right
	}
}

static void GameInpConfig(int nPlayer, int nPcDev, int nAnalog)
{
	struct GameInp* pgi = GameInp;
	unsigned int i;

	for (i = 0; i < nGameInpCount; i++, pgi++) {
		struct BurnInputInfo bii;
		bii.szInfo = NULL;
		BurnDrvGetInputInfo(&bii, i);
		if (bii.pVal == NULL) {
			continue;
		}
		GameInpConfigOne(nPlayer, nPcDev, nAnalog, pgi, (char*)(bii.szInfo ? bii.szInfo : ""));
	}
	for (i = 0; i < nMacroCount; i++, pgi++) {
		GameInpConfigOne(nPlayer, nPcDev, nAnalog, pgi, pgi->Macro.szName);
	}

	GameInpCheckLeftAlt();
}

static int DeleteInput(unsigned int i)
{
	if (i >= nGameInpCount) {
		if (i >= nGameInpCount + nMacroCount) {
			return 1;
		}
		GameInp[i].Macro.nMode = 0;
	} else {
		struct BurnInputInfo bii;
		bii.nType = BIT_DIGITAL;
		bii.pVal = NULL;
		BurnDrvGetInputInfo(&bii, i);
		if (bii.pVal == NULL || (bii.nType & BIT_GROUP_CONSTANT)) {
			return 1;					// dip switches aren't deleted
		}
		GameInp[i].nInput = 0;
	}

	GameInpCheckLeftAlt();
	bSaveInputs = true;
	return 0;
}

// the analog choices: joysticks have "absolute" first, the other devices don't
static int AnalogChoices(const char** pszChoices)
{
	int n = 0;
	if (nPresetDevice >= 1 && nPresetDevice <= 3) {
		pszChoices[n++] = "Absolute";
	}
	pszChoices[n++] = "Auto-center";
	pszChoices[n++] = "Normal";
	return n;
}

static void UsePreset(bool bMakeDefault)
{
	char szFile[MAX_PATH] = "";
	int nDevice = nPresetDevice;
	int nAnalog = 0;

	if (nPresetDevice <= 7) {
		const char* pszChoices[3];
		AnalogChoices(pszChoices);
		nAnalog = nPresetAnalog;
		if (nPresetDevice == 0 || nPresetDevice > 3) {
			nAnalog++;						// no "absolute" in the list
		}
		GameInpConfig(nPresetPlayer, nPresetDevice, nAnalog);
	} else {
		snprintf(szFile, sizeof(szFile), "config/presets/%s.ini", PresetFiles[nPresetDevice - 8].c_str());
		GameInputAutoIni(nPresetPlayer, szFile, true);

		// make sure all the inputs are defined
		for (unsigned int i = 0, j = 0; i < nGameInpCount; i++) {
			if (GameInp[i].Input.pVal == NULL) {
				continue;
			}
			if (GameInp[i].nInput == 0) {
				DeleteInput(j);
			}
			j++;
		}
		nDevice = 0x0F;
	}

	if (bMakeDefault) {
		nPlayerDefaultControls[nPresetPlayer] = nDevice | (nAnalog << 4);
		snprintf(szPlayerDefaultIni[nPresetPlayer], MAX_PATH, "%s", szFile);
	}

	GameInpCheckLeftAlt();
	bSaveInputs = true;
}

static void SaveHardwarePreset()
{
	const char* pszFile = "config/presets/preset.ini";
	const char* pszHardware = "Generic hardware";
	bool bSaveDips = false;
	int nHardware = GameInputGetHWFlag();

	for (INT32 i = 0; gamehw_cfg[i].ini[0] != '\0'; i++) {
		for (INT32 hw = 0; gamehw_cfg[i].hw[hw] != 0; hw++) {
			if (gamehw_cfg[i].hw[hw] == nHardware) {
				pszFile = gamehw_cfg[i].ini;
				pszHardware = gamehw_cfg[i].system;
				bSaveDips = gamehw_cfg[i].dips_in_preset;
				break;
			}
		}
	}

#ifdef _WIN32
	_mkdir("config");
	_mkdir("config/presets");
#endif

	char szText[MAX_PATH + 64];
	FILE* fp = fopen(pszFile, "wt");
	if (fp) {
		fprintf(fp, APP_TITLE " - Hardware Default Preset\n\n");
		fprintf(fp, "%s\n\n", pszHardware);
		fprintf(fp, "version 0x%06X\n\n", nBurnVer);
		GameInpWrite(fp, bSaveDips);
		fclose(fp);
		snprintf(szText, sizeof(szText), "The preset was saved in %s.\n", pszFile);
		FBAPopupAddText(PUF_TEXT_NO_TRANSLATE, (TCHAR*)"%s", szText);
		FBAPopupDisplay(PUF_TYPE_INFO);
	} else {
		snprintf(szText, sizeof(szText), "%s couldn't be written.\n", pszFile);
		AppError(szText, 0);
	}
	ReadPresetFiles();
}

static void DrawPresets()
{
	ImGui::TextDisabled("Presets");

	char szLabel[32];
	snprintf(szLabel, sizeof(szLabel), "Player %d", nPresetPlayer + 1);
	ImGui::SetNextItemWidth(120.0f * fUiScale);
	if (ImGui::BeginCombo("##player", szLabel)) {
		for (int i = 0; i < 4; i++) {
			snprintf(szLabel, sizeof(szLabel), "Player %d", i + 1);
			if (ImGui::Selectable(szLabel, nPresetPlayer == i)) {
				nPresetPlayer = i;
				int nDefault = nPlayerDefaultControls[i] & 0x0F;
				nPresetDevice = nDefault <= 7 ? nDefault : 0;
				nPresetAnalog = 0;
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	ImGui::SetNextItemWidth(220.0f * fUiScale);
	const char* pszDevice = nPresetDevice <= 7 ? pszDevices[nPresetDevice] : PresetFiles[nPresetDevice - 8].c_str();
	if (ImGui::BeginCombo("##device", pszDevice)) {
		for (int i = 0; i < 8; i++) {
			if (ImGui::Selectable(pszDevices[i], nPresetDevice == i)) {
				nPresetDevice = i;
				nPresetAnalog = 0;
			}
		}
		if (!PresetFiles.empty()) {
			ImGui::Separator();
		}
		for (size_t i = 0; i < PresetFiles.size(); i++) {
			if (ImGui::Selectable(PresetFiles[i].c_str(), nPresetDevice == (int)i + 8)) {
				nPresetDevice = i + 8;
			}
		}
		ImGui::EndCombo();
	}

	if (nPresetDevice <= 7) {
		const char* pszChoices[3];
		int nChoices = AnalogChoices(pszChoices);
		if (nPresetAnalog >= nChoices) nPresetAnalog = 0;
		ImGui::SameLine();
		ImGui::SetNextItemWidth(130.0f * fUiScale);
		ImGui::Combo("##analog", &nPresetAnalog, pszChoices, nChoices);
	}

	if (ImGui::Button("Use")) {
		UsePreset(false);
	}
	ImGui::SameLine();
	if (ImGui::Button("Make Default")) {
		UsePreset(true);
	}
	ImGui::SameLine();
	if (ImGui::Button("Save as Hardware Preset")) {
		SaveHardwarePreset();
	}
}

// ----------------------------------------------------------------------------
// The mapping window (inpd.cpp)

static int nMappingSel = -1;
static int nPendingPopup = 0;				// 1 capture, 2 constant: opened at the level of the window, not of the row

static void StateText(struct GameInp* pgi, char* pszText, int nSize)
{
	UINT16 nVal;
	pszText[0] = '\0';

	if (pgi->nType & BIT_GROUP_ANALOG) {
		nVal = bRunPause ? pgi->Input.nVal : *pgi->Input.pShortVal;
	} else {
		nVal = bRunPause ? pgi->Input.nVal : *pgi->Input.pVal;
	}

	switch (pgi->nType) {
		case BIT_DIGITAL:
			if (nVal == 1) snprintf(pszText, nSize, "ON");
			else if (nVal) snprintf(pszText, nSize, "0x%02X", nVal);
			break;
		case BIT_ANALOG_ABS:
			snprintf(pszText, nSize, "0x%02X", nVal >> 8);
			break;
		case BIT_ANALOG_REL:
			if ((INT16)nVal < 0) snprintf(pszText, nSize, "%d", ((INT16)nVal) >> 8);
			if ((INT16)nVal > 0) snprintf(pszText, nSize, "+%d", ((INT16)nVal) >> 8);
			break;
		default:
			snprintf(pszText, nSize, "0x%02X", nVal);
			break;
	}
}

static void Activate(int nInput)
{
	struct BurnInputInfo bii;
	memset(&bii, 0, sizeof(bii));
	int nRet = BurnDrvGetInputInfo(&bii, nInput);
	if (bii.pVal == NULL && nRet != 1) {	// 1: a macro
		return;
	}

	if (nInput < (int)nGameInpCount && (bii.nType & BIT_GROUP_CONSTANT)) {
		nConstInput = nInput;
		nConstValue = GameInp[nInput].Input.Constant.nConst;
		nPendingPopup = 2;
		return;
	}
	if (GameInp[nInput].nInput == GIT_MACRO_CUSTOM) {
		return;
	}

	CaptureStart(nInput);
	nPendingPopup = 1;
}

static void MappingRow(int nInput, const char* pszName, const char* pszMapping, struct GameInp* pgi, bool bMacro)
{
	ImGui::TableNextRow();

	bool bConstant = !bMacro && (pgi->nType & BIT_GROUP_CONSTANT);
	if (bConstant) {
		ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(120, 100, 40, 60));
	}
	if (bMacro) {
		ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, pgi->Macro.nMode ? IM_COL32(140, 50, 50, 70) : IM_COL32(140, 50, 50, 30));
	}

	ImGui::TableNextColumn();
	ImGui::PushID(nInput);
	if (ImGui::Selectable(UiText(pszName), nMappingSel == nInput, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowOverlap)) {
		nMappingSel = nInput;
		if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) || !ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			Activate(nInput);
		}
	}
	if (ImGui::IsItemFocused()) {
		nMappingSel = nInput;
		if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
			DeleteInput(nInput);
		}
	}

	ImGui::TableNextColumn();
	if (strcmp(pszMapping, "code 0x00") == 0) {
		ImGui::TextDisabled("Unassigned (locked)");
	} else {
		ImGui::TextUnformatted(UiText(pszMapping));
	}

	ImGui::TableNextColumn();
	if (!bMacro) {
		char szState[16];
		StateText(pgi, szState, sizeof(szState));
		ImGui::TextUnformatted(szState);
	} else if (pgi->Macro.nSysMacro != 1) {
		// auto-fire: only for the mapped macros of the game
		bool bAuto = pgi->Macro.nSysMacro == 15;
		ImGui::BeginDisabled(pgi->Macro.nMode == 0);
		if (ImGui::Checkbox("##autofire", &bAuto)) {
			pgi->Macro.nSysMacro = bAuto ? 15 : 0;
			bSaveInputs = true;
		}
		ImGui::EndDisabled();
	}
	ImGui::PopID();
}

static void DrawMapping()
{
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x * 0.9f, vp->WorkSize.y * 0.9f), ImGuiCond_Appearing);
	if (!ImGui::BeginPopupModal("Map Game Inputs###mapping", NULL, ImGuiWindowFlags_NoSavedSettings)) {
		return;
	}
	if (!bDrvOkay) {
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}

	ImGuiStyle& style = ImGui::GetStyle();
	float fBottom = ImGui::GetFrameHeightWithSpacing() * 4.0f + ImGui::GetTextLineHeightWithSpacing() + style.ItemSpacing.y * 3.0f;

	ImGui::TextDisabled("Double click or Enter: map the input, Delete: clear it. The last column shows the inputs live, or auto-fire for the macros.");
	if (ImGui::BeginTable("##inputs", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable, ImVec2(0.0f, -fBottom))) {
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Input", ImGuiTableColumnFlags_WidthStretch, 1.2f);
		ImGui::TableSetupColumn("Mapping", ImGuiTableColumnFlags_WidthStretch, 1.6f);
		ImGui::TableSetupColumn("State / Auto-fire", ImGuiTableColumnFlags_WidthFixed, 130.0f * fUiScale);
		ImGui::TableHeadersRow();

		for (unsigned int i = 0; i < nGameInpCount; i++) {
			struct BurnInputInfo bii;
			bii.szName = NULL;
			bii.pVal = NULL;
			BurnDrvGetInputInfo(&bii, i);
			if (bii.pVal == NULL) {
				continue;
			}
			MappingRow(i, bii.szName ? bii.szName : "", InpToDesc(GameInp + i), GameInp + i, false);
		}
		struct GameInp* pgi = GameInp + nGameInpCount;
		for (unsigned int i = 0; i < nMacroCount; i++, pgi++) {
			if (pgi->nInput & GIT_GROUP_MACRO) {
				MappingRow(nGameInpCount + i, pgi->Macro.szName, InpMacroToDesc(pgi), pgi, true);
			}
		}
		ImGui::EndTable();
	}

	if (nPendingPopup) {
		ImGui::OpenPopup(nPendingPopup == 1 ? "Map Input###capture" : "Constant###constant");
		nPendingPopup = 0;
	}
	DrawCapture();
	DrawConstant();

	ImGui::Separator();
	DrawPresets();

	int nPercent = (nAnalogSpeed * 100 + 128) / 256;
	ImGui::SetNextItemWidth(250.0f * fUiScale);
	if (ImGui::SliderInt("Analog sensitivity", &nPercent, 1, 400, "%d%%")) {
		nAnalogSpeed = (int)(nPercent * 256.0 / 100.0 + 0.5);
		bSaveInputs = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Detect Gamepads")) {
		UiInputRedetect();
	}
	ImGui::SameLine();
	ImGui::TextDisabled("%d gamepad%s", nJoystickCount, nJoystickCount == 1 ? "" : "s");

	ImGui::Spacing();
	if (ImGui::Button("Close", ImVec2(120.0f * fUiScale, 0.0f)) || (!UiInputCapturing() && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) && UiEscapePressed())) {
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

// ----------------------------------------------------------------------------
// DIP switches (inpdipsw.cpp keeps the groups)

extern struct GroupOfDIPSwitches GroupDIPSwitchesArray[MAXDIPSWITCHES];
static int nDipGroups = 0;
static std::vector<UINT8> DipBackup;		// the constants when the window opened, for Cancel

static void DipsBackup(bool bRestore)
{
	if (!bRestore) {
		DipBackup.clear();
	}
	for (unsigned int i = 0; i < nGameInpCount; i++) {
		if (GameInp[i].nType & BIT_GROUP_CONSTANT) {
			if (bRestore) {
				if (DipBackup.size() > i) GameInp[i].Input.Constant.nConst = DipBackup[i];
			} else {
				DipBackup.resize(i + 1);
				DipBackup[i] = GameInp[i].Input.Constant.nConst;
			}
		}
	}
}

static void DrawDips()
{
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(UiDialogSize(620.0f, vp->WorkSize.y * 0.85f), ImGuiCond_Appearing);
	if (!ImGui::BeginPopupModal("DIP Switches###dips", NULL, ImGuiWindowFlags_NoSavedSettings)) {
		return;
	}
	if (!bDrvOkay) {
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}

	float fBottom = ImGui::GetFrameHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y * 2.0f;
	if (nDipGroups <= 0) {
		ImGui::TextDisabled("This game has no DIP switches.");
	} else if (ImGui::BeginTable("##dips", 2, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV, ImVec2(0.0f, -fBottom))) {
		ImGui::TableSetupColumn("Switch", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Setting", ImGuiTableColumnFlags_WidthStretch, 1.2f);
		for (int i = 0; i < nDipGroups; i++) {
			GroupOfDIPSwitches* g = &GroupDIPSwitchesArray[i];
			bool bChanged = g->SelectedDIPOption != g->DefaultDIPOption;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			if (bChanged) {
				ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "%s", UiText(g->dipSwitch.szText));
			} else {
				ImGui::TextUnformatted(UiText(g->dipSwitch.szText));
			}
			ImGui::TableNextColumn();
			ImGui::PushID(i);
			ImGui::SetNextItemWidth(-1.0f);
			int nOptions = g->dipSwitch.nSetting < MAXDIPOPTIONS ? g->dipSwitch.nSetting : MAXDIPOPTIONS;
			const char* pszCurrent = g->SelectedDIPOption < nOptions ? g->dipSwitchesOptions[g->SelectedDIPOption].szText : "";
			if (ImGui::BeginCombo("##option", UiText(pszCurrent ? pszCurrent : ""))) {
				for (int j = 0; j < nOptions; j++) {
					const char* pszOption = g->dipSwitchesOptions[j].szText;
					char szLabel[128];
					snprintf(szLabel, sizeof(szLabel), "%s%s", UiText(pszOption ? pszOption : ""), j == g->DefaultDIPOption ? "  (default)" : "");
					ImGui::PushID(j);
					if (ImGui::Selectable(szLabel, j == g->SelectedDIPOption)) {
						setDIPSwitchOption(i, j);
						bSaveInputs = true;
					}
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	ImGui::TextDisabled("Some settings need a reset of the game (Game > Reset).");
	float fButton = 110.0f * fUiScale;
	if (ImGui::Button("OK", ImVec2(fButton, 0.0f))) {
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("Defaults", ImVec2(fButton, 0.0f))) {
		InpDIPSWResetDIPs();
		nDipGroups = InpDIPSWCreate();
		bSaveInputs = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(fButton, 0.0f)) || (!ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) && UiEscapePressed())) {
		DipsBackup(true);
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

// ----------------------------------------------------------------------------
// Cheats (inpcheat.cpp)

static std::vector<int> CheatBackup;
static char szCheatFilter[64] = "";

static bool CheatMatches(const char* pszName)
{
	if (!szCheatFilter[0]) {
		return true;
	}
	for (const char* p = pszName; *p; p++) {
		int i = 0;
		while (szCheatFilter[i] && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)szCheatFilter[i])) i++;
		if (!szCheatFilter[i]) return true;
	}
	return false;
}

static void DrawCheats()
{
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(UiDialogSize(680.0f, vp->WorkSize.y * 0.85f), ImGuiCond_Appearing);
	if (!ImGui::BeginPopupModal("Cheats###cheats", NULL, ImGuiWindowFlags_NoSavedSettings)) {
		return;
	}
	if (!bDrvOkay) {
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}

	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint("##filter", "Filter", szCheatFilter, sizeof(szCheatFilter));

	float fBottom = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
	if (pCheatInfo == NULL) {
		ImGui::TextDisabled("No cheats for this game (support/cheats, cheat.dat).");
	} else if (ImGui::BeginTable("##cheats", 2, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV, ImVec2(0.0f, -fBottom))) {
		ImGui::TableSetupColumn("Cheat", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Option", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		int n = 0;
		for (CheatInfo* pCheat = pCheatInfo; pCheat; pCheat = pCheat->pNext, n++) {
			if (!CheatMatches(pCheat->szCheatName)) {
				continue;
			}
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if (pCheat->pOption[0] == NULL) {		// a heading
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(60, 110, 60, 90));
				ImGui::TextUnformatted(UiText(pCheat->szCheatName));
				continue;
			}
			ImGui::AlignTextToFramePadding();
			if (pCheat->nCurrent) {
				ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "%s", UiText(pCheat->szCheatName));
			} else {
				ImGui::TextUnformatted(UiText(pCheat->szCheatName));
			}
			ImGui::TableNextColumn();
			ImGui::PushID(n);
			ImGui::SetNextItemWidth(-1.0f);
			const char* pszCurrent = pCheat->pOption[pCheat->nCurrent] ? pCheat->pOption[pCheat->nCurrent]->szOptionName : "";
			if (ImGui::BeginCombo("##option", UiText(pszCurrent))) {
				for (int j = 0; j < CHEAT_MAX_OPTIONS && pCheat->pOption[j]; j++) {
					ImGui::PushID(j);
					if (ImGui::Selectable(UiText(pCheat->pOption[j]->szOptionName), j == pCheat->nCurrent)) {
						CheatEnable(n, j);
					}
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	float fButton = 110.0f * fUiScale;
	if (ImGui::Button("OK", ImVec2(fButton, 0.0f))) {
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("Defaults", ImVec2(fButton, 0.0f))) {
		int n = 0;
		for (CheatInfo* pCheat = pCheatInfo; pCheat; pCheat = pCheat->pNext, n++) {
			CheatEnable(n, -1);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(fButton, 0.0f)) || (!ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) && UiEscapePressed())) {
		int n = 0;
		for (CheatInfo* pCheat = pCheatInfo; pCheat && n < (int)CheatBackup.size(); pCheat = pCheat->pNext, n++) {
			if (pCheat->nCurrent != CheatBackup[n]) {
				CheatEnable(n, CheatBackup[n]);
			}
		}
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

// ----------------------------------------------------------------------------

static bool bRedetect = false;

void UiInputRedetect()
{
	bRedetect = true;
}

void UiInputDialogsDraw()
{
	// gamepads plugged or unplugged while a game runs
	if (bDrvOkay && !UiInputCapturing() && (bRedetect || SDL_NumJoysticks() != nJoystickCount)) {
		bRedetect = false;
		InputExit();
		InputInit();
		char szText[48];
		snprintf(szText, sizeof(szText), "%d gamepad%s", nJoystickCount, nJoystickCount == 1 ? "" : "s");
		UpdateMessage(szText);
	}

	if (bMappingRequest) {
		bMappingRequest = false;
		nMappingSel = -1;
		nPresetPlayer = 0;
		nPresetDevice = nPlayerDefaultControls[0] & 0x0F;
		if (nPresetDevice > 7) nPresetDevice = 0;
		nPresetAnalog = 0;
		ReadPresetFiles();
		ImGui::OpenPopup("Map Game Inputs###mapping");
	}
	if (bDipsRequest) {
		bDipsRequest = false;
		nDipGroups = InpDIPSWCreate();
		DipsBackup(false);
		ImGui::OpenPopup("DIP Switches###dips");
	}
	if (bCheatsRequest) {
		bCheatsRequest = false;
		CheatBackup.clear();
		for (CheatInfo* pCheat = pCheatInfo; pCheat; pCheat = pCheat->pNext) {
			CheatBackup.push_back(pCheat->nCurrent);
		}
		szCheatFilter[0] = '\0';
		ImGui::OpenPopup("Cheats###cheats");
	}

	DrawMapping();
	DrawDips();
	DrawCheats();
}
