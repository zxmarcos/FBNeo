// The hotkeys of the interface: the table, the lookup of a key event, the
// editor (Input > Hotkeys) and the lines of the ini
#include "burner.h"
#include "ui.h"
#include "imgui.h"

#define HKMOD_SHIFT		1
#define HKMOD_CTRL		2
#define HKMOD_ALT		4

struct HotkeyBind {
	SDL_Keycode nKey;
	int nMods;
};

struct Hotkey {
	const char* pszName;					// in the ini
	const char* pszGroup;
	const char* pszDesc;
	HotkeyBind Default[2];					// the defaults of the SDL2 build
	HotkeyBind Win32[2];					// the layout of the Windows build
	HotkeyBind Keys[2];
};

#define K(key, mods)	{ key, mods }
#define NONE			{ 0, 0 }

// The order is the one of the UI_HK_ values in ui.h
static Hotkey Hotkeys[UI_HK_COUNT] = {
	{ "Pause",				"Game",			"Pause",											{ K(SDLK_PAUSE, 0), NONE },					{ K(SDLK_PAUSE, 0), NONE } },
	{ "FastForward",		"Game",			"Fast forward (hold), frame advance when paused",	{ K(SDLK_F1, 0), NONE },					{ K(SDLK_F1, 0), NONE } },
	{ "FastForwardToggle",	"Game",			"Fast forward on / off",							{ K(SDLK_F1, HKMOD_SHIFT), NONE },			{ K(SDLK_F1, HKMOD_SHIFT), NONE } },
	{ "Rewind",				"Game",			"Rewind (hold)",									{ K(SDLK_PAGEUP, 0), NONE },				{ K(SDLK_PAGEUP, 0), NONE } },
	{ "GameMenu",			"Game",			"Game menu",										{ K(SDLK_TAB, 0), NONE },					{ K(SDLK_TAB, 0), NONE } },
	{ "LoadGame",			"Game",			"Load game",										{ NONE, NONE },								{ K(SDLK_F6, 0), NONE } },
	{ "EndGame",			"Game",			"End the game (quit when started with a game)",	{ K(SDLK_F12, 0), NONE },					{ K(SDLK_F4, HKMOD_CTRL), K(SDLK_w, HKMOD_CTRL) } },
	{ "StateLoadSlot",		"States",		"Load the state of the active slot",				{ K(SDLK_F9, 0), NONE },					{ K(SDLK_F9, 0), NONE } },
	{ "StateSaveSlot",		"States",		"Save the state to the active slot",				{ K(SDLK_F10, 0), NONE },					{ K(SDLK_F10, 0), NONE } },
	{ "StatePrevSlot",		"States",		"Previous slot",									{ K(SDLK_F8, 0), NONE },					{ K(SDLK_F8, 0), NONE } },
	{ "StateNextSlot",		"States",		"Next slot",										{ K(SDLK_F7, 0), NONE },					{ K(SDLK_F11, 0), NONE } },
	{ "StateUndo",			"States",		"Undo the last save of the slot",					{ K(SDLK_F8, HKMOD_SHIFT), NONE },			{ K(SDLK_F8, HKMOD_SHIFT), NONE } },
	{ "StateLoadDialog",	"States",		"Load a state file",								{ K(SDLK_F9, HKMOD_CTRL), NONE },			{ K(SDLK_F9, HKMOD_CTRL), NONE } },
	{ "StateSaveDialog",	"States",		"Save a state file",								{ K(SDLK_F10, HKMOD_CTRL), NONE },			{ K(SDLK_F10, HKMOD_CTRL), NONE } },
	{ "MemcardToggle",		"Memory card",	"Insert / eject the memory card",					{ K(SDLK_F7, HKMOD_SHIFT), NONE },			{ K(SDLK_F7, 0), NONE } },
	{ "MapInputs",			"Input",		"Map the game inputs",								{ K(SDLK_F5, 0), NONE },					{ K(SDLK_F5, 0), NONE } },
	{ "DipSwitches",		"Input",		"DIP switches",										{ K(SDLK_F5, HKMOD_CTRL), NONE },			{ K(SDLK_F5, HKMOD_CTRL), NONE } },
	{ "Cheats",				"Input",		"Cheats",											{ K(SDLK_F5, HKMOD_SHIFT), NONE },			{ NONE, NONE } },
	{ "FullScreen",			"Video",		"Fullscreen",										{ K(SDLK_RETURN, HKMOD_ALT), NONE },		{ K(SDLK_RETURN, HKMOD_ALT), NONE } },
	{ "ToggleMenu",			"Video",		"Show / hide the menu bar",							{ K(SDLK_RETURN, HKMOD_CTRL), NONE },		{ K(SDLK_RETURN, HKMOD_CTRL), NONE } },
	{ "ToggleFPS",			"Video",		"Show / hide the FPS",								{ K(SDLK_F11, 0), NONE },					{ K(SDLK_BACKSPACE, 0), NONE } },
	{ "SaveSnapshot",		"Video",		"Save a screenshot",								{ K(SDLK_F6, 0), NONE },					{ K(SDLK_F12, 0), NONE } },
	{ "Mute",				"Sound",		"Mute / unmute",									{ K(SDLK_s, HKMOD_ALT), NONE },				{ K(SDLK_s, HKMOD_ALT), NONE } },
	{ "VolumeUp",			"Sound",		"Volume up",										{ K(SDLK_KP_PLUS, 0), NONE },				{ K(SDLK_EQUALS, HKMOD_ALT), NONE } },
	{ "VolumeDown",			"Sound",		"Volume down",										{ K(SDLK_KP_MINUS, 0), NONE },				{ K(SDLK_MINUS, HKMOD_ALT), NONE } },
};

#undef K
#undef NONE

static bool bDefaultsSet = false;

static void SetDefaults(bool bWin32)
{
	for (int i = 0; i < UI_HK_COUNT; i++) {
		for (int j = 0; j < 2; j++) {
			Hotkeys[i].Keys[j] = bWin32 ? Hotkeys[i].Win32[j] : Hotkeys[i].Default[j];
		}
	}
	bDefaultsSet = true;
}

static void CheckDefaults()
{
	if (!bDefaultsSet) {
		SetDefaults(false);
	}
}

static int EventMods(Uint16 nMod)
{
	return ((nMod & KMOD_SHIFT) ? HKMOD_SHIFT : 0) | ((nMod & KMOD_CTRL) ? HKMOD_CTRL : 0) | ((nMod & KMOD_ALT) ? HKMOD_ALT : 0);
}

int UiHotkeyFind(const SDL_KeyboardEvent& key)
{
	CheckDefaults();

	int nMods = EventMods(key.keysym.mod);
	for (int i = 0; i < UI_HK_COUNT; i++) {
		for (int j = 0; j < 2; j++) {
			if (Hotkeys[i].Keys[j].nKey && Hotkeys[i].Keys[j].nKey == key.keysym.sym && Hotkeys[i].Keys[j].nMods == nMods) {
				return i;
			}
		}
	}
	return -1;
}

// for the release of a hold hotkey, whatever the modifiers are then
bool UiHotkeyUsesKey(int nAction, SDL_Keycode nKey)
{
	CheckDefaults();

	for (int j = 0; j < 2; j++) {
		if (Hotkeys[nAction].Keys[j].nKey && Hotkeys[nAction].Keys[j].nKey == nKey) {
			return true;
		}
	}
	return false;
}

static const char* BindName(const HotkeyBind& bind, char* pszText, int nSize)
{
	if (bind.nKey == 0) {
		snprintf(pszText, nSize, "%s", "");
		return pszText;
	}
	snprintf(pszText, nSize, "%s%s%s%s", (bind.nMods & HKMOD_CTRL) ? "Ctrl+" : "", (bind.nMods & HKMOD_ALT) ? "Alt+" : "", (bind.nMods & HKMOD_SHIFT) ? "Shift+" : "", SDL_GetKeyName(bind.nKey));
	return pszText;
}

// the first key of an action, for the menus
const char* UiHotkeyName(int nAction)
{
	static char szNames[4][64];
	static int nNext = 0;

	CheckDefaults();

	char* pszText = szNames[nNext];
	nNext = (nNext + 1) & 3;

	const HotkeyBind& bind = Hotkeys[nAction].Keys[0].nKey ? Hotkeys[nAction].Keys[0] : Hotkeys[nAction].Keys[1];
	return BindName(bind, pszText, sizeof(szNames[0]));
}

// ----------------------------------------------------------------------------
// ini: "Hotkey <name> <key> <mods> <key> <mods>", with the SDL key codes

void UiHotkeysSave(FILE* fp)
{
	CheckDefaults();

	fprintf(fp, "\n// Hotkeys: SDL key code and modifiers (1 shift, 2 ctrl, 4 alt), twice\n");
	for (int i = 0; i < UI_HK_COUNT; i++) {
		char szA[64], szB[64];
		fprintf(fp, "Hotkey %s 0x%X %d 0x%X %d\t\t// %s %s\n", Hotkeys[i].pszName, (unsigned int)Hotkeys[i].Keys[0].nKey, Hotkeys[i].Keys[0].nMods,
			(unsigned int)Hotkeys[i].Keys[1].nKey, Hotkeys[i].Keys[1].nMods, BindName(Hotkeys[i].Keys[0], szA, sizeof(szA)), BindName(Hotkeys[i].Keys[1], szB, sizeof(szB)));
	}
}

bool UiHotkeysLoadLine(const char* pszLine)
{
	CheckDefaults();

	if (strncmp(pszLine, "Hotkey ", 7) != 0) {
		return false;
	}

	char szName[64];
	unsigned int nKey0 = 0, nKey1 = 0;
	int nMods0 = 0, nMods1 = 0;
	if (sscanf(pszLine + 7, "%63s %x %d %x %d", szName, &nKey0, &nMods0, &nKey1, &nMods1) < 3) {
		return true;
	}
	for (int i = 0; i < UI_HK_COUNT; i++) {
		if (strcmp(Hotkeys[i].pszName, szName) == 0) {
			Hotkeys[i].Keys[0].nKey = nKey0;
			Hotkeys[i].Keys[0].nMods = nMods0;
			Hotkeys[i].Keys[1].nKey = nKey1;
			Hotkeys[i].Keys[1].nMods = nMods1;
			break;
		}
	}
	return true;
}

// ----------------------------------------------------------------------------
// The editor

static bool bEditorRequest = false;
static int nCaptureAction = -1;				// the binding waiting for a key
static int nCaptureSlot = 0;
static char szConflict[160] = "";
static int nCaptureEndFrame = -10;			// the Escape that ended a capture doesn't close the editor

void UiHotkeysOpen()
{
	bEditorRequest = true;
}

bool UiHotkeysCapturing()
{
	return nCaptureAction >= 0;
}

// The key events go here first while a binding waits for its key
void UiHotkeysCaptureKey(const SDL_KeyboardEvent& key)
{
	SDL_Keycode nKey = key.keysym.sym;

	switch (nKey) {
		case SDLK_LSHIFT: case SDLK_RSHIFT: case SDLK_LCTRL: case SDLK_RCTRL:
		case SDLK_LALT: case SDLK_RALT: case SDLK_LGUI: case SDLK_RGUI:
			return;							// a modifier alone, wait for the key
	}

	nCaptureEndFrame = ImGui::GetFrameCount();
	if (nKey == SDLK_ESCAPE && EventMods(key.keysym.mod) == 0) {
		nCaptureAction = -1;				// Escape cancels
		return;
	}

	HotkeyBind bind = { nKey, EventMods(key.keysym.mod) };
	szConflict[0] = '\0';

	// a key can only do one thing: the action that had it loses it
	for (int i = 0; i < UI_HK_COUNT; i++) {
		for (int j = 0; j < 2; j++) {
			if ((i != nCaptureAction || j != nCaptureSlot) && Hotkeys[i].Keys[j].nKey == bind.nKey && Hotkeys[i].Keys[j].nMods == bind.nMods) {
				char szName[64];
				snprintf(szConflict, sizeof(szConflict), "%s was taken from \"%s\".", BindName(bind, szName, sizeof(szName)), Hotkeys[i].pszDesc);
				Hotkeys[i].Keys[j].nKey = 0;
				Hotkeys[i].Keys[j].nMods = 0;
			}
		}
	}

	Hotkeys[nCaptureAction].Keys[nCaptureSlot] = bind;
	nCaptureAction = -1;
}

static void BindButton(int nAction, int nSlot)
{
	char szText[96], szName[64];
	ImGui::PushID(nAction * 2 + nSlot);
	if (nCaptureAction == nAction && nCaptureSlot == nSlot) {
		snprintf(szText, sizeof(szText), "Press a key...###bind");
	} else {
		const char* pszName = BindName(Hotkeys[nAction].Keys[nSlot], szName, sizeof(szName));
		snprintf(szText, sizeof(szText), "%s###bind", pszName[0] ? pszName : "-");
	}
	if (ImGui::Button(szText, ImVec2(-ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x, 0.0f))) {
		nCaptureAction = nAction;
		nCaptureSlot = nSlot;
		szConflict[0] = '\0';
	}
	ImGui::SameLine();
	if (ImGui::Button("X", ImVec2(ImGui::GetFrameHeight(), 0.0f))) {
		Hotkeys[nAction].Keys[nSlot].nKey = 0;
		Hotkeys[nAction].Keys[nSlot].nMods = 0;
		nCaptureAction = -1;
	}
	ImGui::PopID();
}

void UiHotkeysDraw()
{
	CheckDefaults();

	if (bEditorRequest) {
		bEditorRequest = false;
		nCaptureAction = -1;
		szConflict[0] = '\0';
		ImGui::OpenPopup("Hotkeys###hotkeys");
	}

	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(UiDialogSize(720.0f, vp->WorkSize.y * 0.85f), ImGuiCond_Appearing);
	if (!ImGui::BeginPopupModal("Hotkeys###hotkeys", NULL, ImGuiWindowFlags_NoSavedSettings)) {
		nCaptureAction = -1;
		return;
	}

	ImGui::TextWrapped("Click a key to change it, then press the new key (with Ctrl, Alt or Shift if wanted). Escape cancels.");

	float fBottom = ImGui::GetFrameHeightWithSpacing() * 2.0f + ImGui::GetStyle().ItemSpacing.y;
	if (ImGui::BeginTable("##hotkeys", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV, ImVec2(0.0f, -fBottom))) {
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 2.0f);
		ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Alternative", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableHeadersRow();

		const char* pszGroup = "";
		for (int i = 0; i < UI_HK_COUNT; i++) {
			if (strcmp(pszGroup, Hotkeys[i].pszGroup) != 0) {
				pszGroup = Hotkeys[i].pszGroup;
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextDisabled("%s", pszGroup);
			}
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(Hotkeys[i].pszDesc);
			ImGui::TableNextColumn();
			BindButton(i, 0);
			ImGui::TableNextColumn();
			BindButton(i, 1);
		}
		ImGui::EndTable();
	}

	if (szConflict[0]) {
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "%s", szConflict);
	} else {
		ImGui::NewLine();
	}

	float fButton = 160.0f * fUiScale;
	if (ImGui::Button("SDL2 Defaults", ImVec2(fButton, 0.0f))) {
		SetDefaults(false);
		szConflict[0] = '\0';
	}
	ImGui::SameLine();
	if (ImGui::Button("Windows Layout", ImVec2(fButton, 0.0f))) {
		SetDefaults(true);
		szConflict[0] = '\0';
	}
	ImGui::SameLine();
	if (ImGui::Button("Close", ImVec2(fButton, 0.0f)) || (nCaptureAction < 0 && ImGui::GetFrameCount() > nCaptureEndFrame + 1 && UiEscapePressed())) {
		nCaptureAction = -1;
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

// The list of the Help menu
void UiHotkeysTable()
{
	CheckDefaults();

	if (ImGui::BeginTable("##keys", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
		for (int i = 0; i < UI_HK_COUNT; i++) {
			char szA[64], szB[64];
			BindName(Hotkeys[i].Keys[0], szA, sizeof(szA));
			BindName(Hotkeys[i].Keys[1], szB, sizeof(szB));
			if (!szA[0] && !szB[0]) {
				continue;
			}
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%s%s%s", szA, (szA[0] && szB[0]) ? " / " : "", szB);
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(Hotkeys[i].pszDesc);
		}
		ImGui::EndTable();
	}
}
