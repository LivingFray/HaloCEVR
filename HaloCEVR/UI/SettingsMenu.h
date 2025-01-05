#pragma once
#include <d3d9.h>
#include <string>
#include "UIRenderer.h"

class SettingsMenu
{
public:
	SettingsMenu() = default;

	void CreateMenus();

	void ShowMenus(bool bShow);

	void NextPage(std::vector<class UIPanel*>& pages, int& currentPage);
	void PrevPage(std::vector<class UIPanel*>& pages, int& currentPage);
	void SetPage(std::vector<class UIPanel*>& pages, int& page);

	void ReloadSettingsPages();

	bool bVisible = false;

	int basicSettingsPage = 0;
	std::vector<class UIPanel*> basicSettingsPages;
	int allSettingsPage = 0;
	std::vector<class UIPanel*> allSettingsPages;

protected:

	static inline D3DCOLOR textLabelColour = D3DCOLOR_ARGB(255, 33, 150, 255);
	static inline D3DCOLOR textValueColour = D3DCOLOR_ARGB(255, 255, 255, 255);
	static inline D3DCOLOR backgroundColour = D3DCOLOR_ARGB(255, 0, 20, 35);
	static inline D3DCOLOR darkPanelColour = D3DCOLOR_ARGB(255, 3, 11, 44);
	static inline D3DCOLOR mediumPanelColour = D3DCOLOR_ARGB(255, 6, 21, 51);
	static inline D3DCOLOR lightPanelColour = D3DCOLOR_ARGB(255, 8, 29, 58);

	static inline float settingArrowWidth = 32.0f;
	static inline float settingPanelPaddingX = 10.0f;
	static inline float settingPanelPaddingY = 10.0f;
	static inline float settingPanelSaveButtonY = 50.0f;

	IDirect3DTexture9* settingsButtonImage = nullptr;
	IDirect3DTexture9* prevPageButtonImage = nullptr;
	IDirect3DTexture9* nextPageButtonImage = nullptr;

	class UIPanel* mainPanel = nullptr;
	class UIPanel* backgroundPanel = nullptr;
	class UITabPanel* settingsTabs = nullptr;
	class UIPanel* basicSettingsPanel = nullptr;
	class UIPanel* allSettingsPanel = nullptr;

	void GenerateSettingsTab(const std::vector<std::string>& settingNames, class UIPanel* parentPanel, std::vector<class UIPanel*>& pages);
	class UIPanel* GenerateSettingsPanel(std::string propertyName, class UIPanel* parentPanel, float y, float padding, float innerPadding);
	
	void PopulateBasicSettingsTab();
	void PopulateAllSettingsTab();
	void PopulateAboutTab(class UIPanel* aboutPanel);
};

