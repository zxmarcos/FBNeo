// Message boxes (FBAPopup*, AppError) and the loading progress window
#include "burner.h"
#include "ui.h"
#include "imgui.h"
#include "imgui_internal.h"

#define POPUP_MAX 16

struct Popup {
	int nType;
	char* pszText;
};

static Popup Popups[POPUP_MAX];
static int nPopupCount = 0;

// text added since the last FBAPopupDisplay() / UiPopupFlush()
static char* pszPending = NULL;
static int nPendingLen = 0;
static int nPendingSize = 0;
static int nPendingType = 0;

static void PendingAdd(const char* pszText)
{
	int nLen = strlen(pszText);
	if (nPendingLen + nLen + 1 > nPendingSize) {
		int nNewSize = (nPendingLen + nLen + 1) * 2;
		char* pszNew = (char*)realloc(pszPending, nNewSize);
		if (pszNew == NULL) {
			return;
		}
		pszPending = pszNew;
		nPendingSize = nNewSize;
	}
	memcpy(pszPending + nPendingLen, pszText, nLen + 1);
	nPendingLen += nLen;
}

static void PendingType(int nType)
{
	// keep the most severe: error, then warning, then information
	if (nPendingType == 0 || nType == PUF_TYPE_ERROR || (nType == PUF_TYPE_WARNING && nPendingType == PUF_TYPE_INFO)) {
		nPendingType = nType;
	}
}

static void PendingClear()
{
	nPendingLen = 0;
	nPendingType = 0;
	if (pszPending) {
		pszPending[0] = '\0';
	}
}

static void PopupPush(int nType, const char* pszText)
{
	fprintf(stderr, "%s", pszText);

	if (nPopupCount >= POPUP_MAX) {
		return;
	}

	// drop the trailing line feeds
	int nLen = strlen(pszText);
	while (nLen > 0 && (pszText[nLen - 1] == '\n' || pszText[nLen - 1] == '\r')) {
		nLen--;
	}
	char* pszCopy = (char*)malloc(nLen + 1);
	if (pszCopy == NULL) {
		return;
	}
	memcpy(pszCopy, pszText, nLen);
	pszCopy[nLen] = '\0';

	Popups[nPopupCount].nType = nType;
	Popups[nPopupCount].pszText = pszCopy;
	nPopupCount++;
}

int FBAPopupAddText(int nFlags, TCHAR* pszFormat, ...)
{
	char szText[4096];
	va_list args;

	va_start(args, pszFormat);
	vsnprintf(szText, sizeof(szText), pszFormat, args);
	va_end(args);

	PendingAdd(szText);

	return 0;
}

int FBAPopupDisplay(int nFlags)
{
	int nType = nFlags & 7;

	if (nPendingLen) {
		if (nFlags & PUF_TYPE_LOGONLY) {
			fprintf(stderr, "%s", pszPending);
		} else {
			PopupPush(nType ? nType : PUF_TYPE_INFO, pszPending);
		}
	}
	PendingClear();

	return 0;
}

int FBAPopupDestroyText()
{
	PendingClear();

	return 0;
}

// Errors are gathered and shown together once the current operation is over,
// so loading a game with many missing roms gives one message
int AppError(TCHAR* szText, int bWarning)
{
	if (szText == NULL || szText[0] == '\0') {
		return 0;
	}

	PendingAdd(szText);
	if (szText[strlen(szText) - 1] != '\n') {
		PendingAdd("\n");
	}
	PendingType(bWarning ? PUF_TYPE_WARNING : PUF_TYPE_ERROR);

	return 0;
}

void UiPopupFlush()
{
	if (nPendingLen) {
		PopupPush(nPendingType ? nPendingType : PUF_TYPE_ERROR, pszPending);
	}
	PendingClear();
}

bool UiPopupActive()
{
	return nPopupCount > 0;
}

void UiPopupsDraw()
{
	if (nPopupCount == 0) {
		return;
	}

	Popup* p = &Popups[0];
	const char* pszTitle = "Information###fbneo_popup";
	if (p->nType == PUF_TYPE_ERROR) pszTitle = "Error###fbneo_popup";
	if (p->nType == PUF_TYPE_WARNING) pszTitle = "Warning###fbneo_popup";

	if (!ImGui::IsPopupOpen("###fbneo_popup")) {
		ImGui::OpenPopup("###fbneo_popup");
	}

	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal(pszTitle, NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
		float fWrap = 520.0f * fUiScale;
		if (fWrap > vp->WorkSize.x * 0.8f) {
			fWrap = vp->WorkSize.x * 0.8f;
		}

		// long texts (a list of missing roms) scroll inside the box
		ImVec2 size = ImGui::CalcTextSize(p->pszText, NULL, false, fWrap);
		float fMaxHeight = vp->WorkSize.y * 0.6f;
		if (size.y > fMaxHeight) {
			ImGui::BeginChild("##text", ImVec2(fWrap + ImGui::GetStyle().ScrollbarSize, fMaxHeight));
		} else {
			ImGui::BeginChild("##text", ImVec2(fWrap, size.y));
		}
		ImGui::PushTextWrapPos(fWrap);
		ImGui::TextUnformatted(p->pszText);
		ImGui::PopTextWrapPos();
		ImGui::EndChild();

		ImGui::Spacing();
		float fButton = 120.0f * fUiScale;
		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - fButton) * 0.5f);
		bool bClose = ImGui::Button("OK", ImVec2(fButton, 0));
		ImGui::SetItemDefaultFocus();
		if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
			bClose = true;
		}

		if (bClose) {
			free(p->pszText);
			nPopupCount--;
			memmove(&Popups[0], &Popups[1], nPopupCount * sizeof(Popup));
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

// ----------------------------------------------------------------------------
// Progress, shown while a game loads. The interface loop is blocked by
// DrvInit() then, so this draws whole frames by itself.

static bool bProgress = false;
static char szProgressTitle[256];
static char szProgressText[256];
static double dPosBurner = 0.0;			// the frontend part (rom loading), 0 to 1
static double dPosBurn = 0.0;			// the driver part (decryption...), 0 to dRangeBurn
static double dRangeBurn = 0.0;
static Uint32 nLastDraw = 0;

static void ProgressDraw()
{
	// only between two interface frames
	if (ImGui::GetCurrentContext() == NULL || GImGui->WithinFrameScope) {
		return;
	}

	Uint32 nNow = SDL_GetTicks();
	if (nNow - nLastDraw < 50) {
		return;
	}
	nLastDraw = nNow;

	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		UiProcessEvent(&e);
		if (e.type == SDL_QUIT) {
			UiQuit();
		}
	}

	UiNewFrame();

	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(440.0f * fUiScale, 0.0f));
	ImGui::Begin("##progress", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
	ImGui::TextUnformatted(szProgressTitle);
	ImGui::Spacing();
	ImGui::ProgressBar((float)((dPosBurner + dPosBurn) / (1.0 + dRangeBurn)), ImVec2(-1.0f, 0.0f));
	ImGui::TextUnformatted(szProgressText);
	ImGui::End();

	UiPresent(false);
}

static int __cdecl ProgressSetRangeBurn(double dProgressRange)
{
	dRangeBurn = dProgressRange;
	dPosBurn = 0.0;

	return 0;
}

static int __cdecl ProgressUpdateBurn(double dProgress, const TCHAR* pszText, bool bAbs)
{
	if (!bProgress) {
		return 0;
	}

	if (pszText) {
		snprintf(szProgressText, sizeof(szProgressText), "%s", UiText(pszText));
	}
	dPosBurn = (bAbs ? 0.0 : dPosBurn) + dRangeBurn * dProgress;
	if (dPosBurn > dRangeBurn) {
		dPosBurn = dRangeBurn;
	}
	ProgressDraw();

	return 0;
}

int ProgressUpdateBurner(double dProgress, const TCHAR* pszText, bool bAbs)
{
	if (!bProgress) {
		return 0;
	}

	if (pszText) {
		snprintf(szProgressText, sizeof(szProgressText), "%s", UiText(pszText));
	}
	dPosBurner = (bAbs ? 0.0 : dPosBurner) + dProgress;
	if (dPosBurner > 1.0) {
		dPosBurner = 1.0;
	}
	ProgressDraw();

	return 0;
}

void UiProgressStart(const char* pszTitle)
{
	snprintf(szProgressTitle, sizeof(szProgressTitle), "%s", pszTitle);
	szProgressText[0] = '\0';
	dPosBurner = dPosBurn = dRangeBurn = 0.0;
	nLastDraw = 0;
	bProgress = true;

	BurnExtProgressRangeCallback = ProgressSetRangeBurn;
	BurnExtProgressUpdateCallback = ProgressUpdateBurn;

	ProgressDraw();
}

void UiProgressEnd()
{
	bProgress = false;

	BurnExtProgressRangeCallback = NULL;
	BurnExtProgressUpdateCallback = NULL;
}
