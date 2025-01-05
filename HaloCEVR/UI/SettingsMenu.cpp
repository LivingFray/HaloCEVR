#include <sstream>
#include "SettingsMenu.h"
#include "../Game.h"
#include "UIRenderer.h"
#include "UIElement.h"
#include "UILabel.h"
#include "UIPanel.h"
#include "UITabPanel.h"
#include "UITextButton.h"
#include "UICheckbox.h"
#include "UIImageButton.h"
#include "UIIntBox.h"
#include "UIFloatBox.h"
#include "../Helpers/Version.h"

// TODO: Hook this up to some sort of github action
#define MOD_VERSION "1.3.0"

void SettingsMenu::CreateMenus()
{
	UIRenderer* uiRenderer = Game::instance.uiRenderer;

	settingsButtonImage = uiRenderer->LoadTexture("VR/Images/settings.png");
	prevPageButtonImage = uiRenderer->LoadTexture("VR/Images/left.png");
	nextPageButtonImage = uiRenderer->LoadTexture("VR/Images/right.png");

	UIImageButton* settingsButton = new UIImageButton(0.0f, 0.0f, 50.0f, 50.0f, settingsButtonImage);
	settingsButton->bToggle = true;
	settingsButton->onClick = [settingsButton, this]() {
		this->ShowMenus(settingsButton->bIsActive);
	};

	uiRenderer->AddElement(settingsButton);

	mainPanel = new UIPanel(0.0f, 0.0f, 640.0f, 480.0f, 0, false);
	uiRenderer->AddElement(mainPanel);

	mainPanel->AddElement(new UILabel(60.0f, 0.0f, "VR Settings", uiRenderer->GetTitleFont(), textLabelColour));
	mainPanel->bVisible = false;

	backgroundPanel = new UIPanel(0.0f, 60.0f, 640.0f, 420.0f, backgroundColour);
	mainPanel->AddElement(backgroundPanel);


	// This should really be done more automatically
	const float tabWidth = uiRenderer->CalculateStringWidth(*uiRenderer->GetLargeFont(), " Basic Settings ");
	const float tabHeight = uiRenderer->GetLargeFont()->lineHeight * 1.1f;

	settingsTabs = new UITabPanel(0.0f, 0.0f, backgroundPanel->w, tabHeight);
	backgroundPanel->AddElement(settingsTabs);

	// Basic settings tab
	UITextButton* basicSettings = new UITextButton(20.0f, 0.0f, tabWidth, tabHeight, "Basic Settings", uiRenderer->GetLargeFont(), darkPanelColour, lightPanelColour, textLabelColour);
	basicSettingsPanel = new UIPanel(0.0f, tabHeight, backgroundPanel->w, backgroundPanel->h - tabHeight, lightPanelColour);
	settingsTabs->AddTab(basicSettings, basicSettingsPanel);
	backgroundPanel->AddElement(basicSettingsPanel);

	PopulateBasicSettingsTab();

	// All settings tab
	UITextButton* allSettings = new UITextButton(20.0f, 0.0f, tabWidth, tabHeight, "All Settings", uiRenderer->GetLargeFont(), darkPanelColour, lightPanelColour, textLabelColour);
	allSettingsPanel = new UIPanel(0.0f, tabHeight, backgroundPanel->w, backgroundPanel->h - tabHeight, lightPanelColour);
	settingsTabs->AddTab(allSettings, allSettingsPanel);
	backgroundPanel->AddElement(allSettingsPanel);

	PopulateAllSettingsTab();

	// About tab
	UITextButton* aboutTab = new UITextButton(0.0f, 0.0f, tabWidth, tabHeight, "About", uiRenderer->GetLargeFont(), darkPanelColour, lightPanelColour, textLabelColour);
	UIPanel* aboutPanel = new UIPanel(0.0f, tabHeight, backgroundPanel->w, backgroundPanel->h - tabHeight, lightPanelColour);
	settingsTabs->AddTab(aboutTab, aboutPanel);
	backgroundPanel->AddElement(aboutPanel);
	PopulateAboutTab(aboutPanel);

	// Patch notes tab

	// Logs tab
}

void SettingsMenu::ShowMenus(bool bShow)
{
	mainPanel->bVisible = bShow;

	bVisible = bShow;

	if (!bShow)
	{
		Game::instance.uiRenderer->UpdateActiveButton(nullptr);

		Game::instance.config.SaveToFile("VR/config.txt");
	}
	else
	{
		Game::instance.config.SaveToFile("VR/config.backup.txt");
	}
}

void SettingsMenu::NextPage(std::vector<class UIPanel*>& pages, int& currentPage)
{
	currentPage = (currentPage + 1) % pages.size();

	for (size_t i = 0; i < pages.size(); i++)
	{
		pages[i]->bVisible = currentPage == i;
	}
}

void SettingsMenu::PrevPage(std::vector<class UIPanel*>& pages, int& currentPage)
{
	currentPage = (currentPage - 1 + pages.size()) % pages.size();

	for (size_t i = 0; i < pages.size(); i++)
	{
		pages[i]->bVisible = currentPage == i;
	}
}

void SettingsMenu::SetPage(std::vector<class UIPanel*>& pages, int& page)
{
	for (size_t i = 0; i < pages.size(); i++)
	{
		pages[i]->bVisible = page == i;
	}
}

void SettingsMenu::ReloadSettingsPages()
{
	PopulateBasicSettingsTab();
	SetPage(basicSettingsPages, basicSettingsPage);
	PopulateAllSettingsTab();
	SetPage(allSettingsPages, allSettingsPage);
}

void SettingsMenu::GenerateSettingsTab(const std::vector<std::string>& settingNames, UIPanel* parentPanel, std::vector<class UIPanel*>& pages)
{
	const float initY = settingPanelPaddingY;
	const float paddingX = (settingPanelPaddingX + settingArrowWidth) * 2.0f;
	const float paddingY = settingPanelPaddingY;
	const float paddingInner = 10.0f;

	const float buttonsWidth = Game::instance.uiRenderer->CalculateStringWidth(*Game::instance.uiRenderer->GetMediumFont(), " Revert ");

	UITextButton* saveButton = new UITextButton(parentPanel->w - paddingX - buttonsWidth * 2.0f - paddingInner,
		parentPanel->h - settingPanelSaveButtonY + paddingInner,
		buttonsWidth,
		settingPanelSaveButtonY - paddingInner * 2.0f,
		"Save",
		Game::instance.uiRenderer->GetMediumFont(),
		mediumPanelColour,
		mediumPanelColour,
		textValueColour
	);
	saveButton->onClick = []() {
		Game::instance.config.SaveToFile("VR/config.txt");
		Game::instance.config.SaveToFile("VR/config.backup.txt");
	};
	parentPanel->AddElement(saveButton);

	UITextButton* revertButton = new UITextButton(parentPanel->w - paddingX - buttonsWidth,
		parentPanel->h - settingPanelSaveButtonY + paddingInner,
		buttonsWidth,
		settingPanelSaveButtonY - paddingInner * 2.0f,
		"Revert",
		Game::instance.uiRenderer->GetMediumFont(),
		mediumPanelColour,
		mediumPanelColour,
		textValueColour
	);
	revertButton->onClick = [this]() {
		Game::instance.config.LoadFromFile("VR/config.backup.txt");
		this->ReloadSettingsPages();
	};
	parentPanel->AddElement(revertButton);

	UIPanel* currentPanel = new UIPanel(0.0f, 0.0f, parentPanel->w, parentPanel->h - settingPanelSaveButtonY, parentPanel->color, parentPanel->bDrawBackground);
	pages.push_back(currentPanel);
	parentPanel->AddElement(currentPanel);

	float y = initY;

	for (size_t i = 0; i < settingNames.size(); i++)
	{
		const std::string& setting = settingNames[i];
		UIPanel* newPanel = GenerateSettingsPanel(setting, currentPanel, y, paddingX, paddingInner);
		if (newPanel)
		{
			y += newPanel->h + paddingY;

			// TODO: This is a bit wasteful and should be refactored at some point
			if (y > currentPanel->h)
			{
				// Too far, discard last setting, put it on the next page
				i--;
				y = initY;
				currentPanel->RemoveElement(newPanel);
				delete newPanel;

				currentPanel = new UIPanel(0.0f, 0.0f, parentPanel->w, parentPanel->h - settingPanelSaveButtonY, parentPanel->color, parentPanel->bDrawBackground);
				currentPanel->bVisible = false;
				pages.push_back(currentPanel);
				parentPanel->AddElement(currentPanel);
			}
		}
	}
}

UIPanel* SettingsMenu::GenerateSettingsPanel(std::string propertyName, class UIPanel* parentPanel, float y, float padding, float innerPadding)
{
	UIRenderer* uiRenderer = Game::instance.uiRenderer;

	Property* propertyValue = Game::instance.config.Get(propertyName);

	if (!propertyValue)
	{
		Logger::log << "[Settings] Couldn't find property " << propertyName << ". Skipping in settings UI" << std::endl;
		return nullptr;
	}

	float panelHeight = 0.0f;

	UIPanel* settingPanel = new UIPanel(padding * 0.5f, y, parentPanel->w - padding, panelHeight, darkPanelColour);

	parentPanel->AddElement(settingPanel);

	Font* medFont = uiRenderer->GetMediumFont();
	Font* smallFont = uiRenderer->GetSmallFont();
	Font* descFont = uiRenderer->GetDescriptionFont();

	settingPanel->AddElement(new UILabel(innerPadding, innerPadding, propertyName, medFont, textLabelColour));
	panelHeight += medFont->lineHeight + innerPadding;

	std::string description;
	float descriptionHeight = uiRenderer->CalculateWrappedString(*descFont, propertyValue->GetDesc().c_str(), settingPanel->w - innerPadding, description);

	settingPanel->AddElement(new UILabel(innerPadding, panelHeight, description, descFont, textValueColour));
	panelHeight += descriptionHeight + innerPadding;

	float labelHeight = medFont->lineHeight;

	if (BoolProperty* boolProperty = dynamic_cast<BoolProperty*>(propertyValue))
	{
		float checkSize = smallFont->lineHeight;
		float checkPadding = (labelHeight - checkSize) * 0.5f + innerPadding;
		UICheckbox* button = new UICheckbox(settingPanel->w - checkSize - innerPadding, checkPadding, checkSize, boolProperty->Value(), mediumPanelColour, textValueColour);
		
		button->onClick = [boolProperty, button]() {
			boolProperty->SetValue(button->bIsActive);
		};
		
		settingPanel->AddElement(button);
	}
	else if (IntProperty* intProperty = dynamic_cast<IntProperty*>(propertyValue))
	{
		float boxWidth = uiRenderer->CalculateStringWidth(*smallFont, "999999999");
		float boxHeight = smallFont->lineHeight * 1.1f;
		float boxPadding = (labelHeight - boxHeight) * 0.5f + innerPadding;
		UIIntBox* button = new UIIntBox(settingPanel->w - boxWidth - innerPadding, boxPadding, boxWidth, boxHeight, intProperty->Value(), smallFont, mediumPanelColour, lightPanelColour, textValueColour);

		button->onClick = [intProperty, button]() {
			intProperty->SetValue(button->intValue);
		};

		settingPanel->AddElement(button);
	}
	else if (FloatProperty* floatProperty = dynamic_cast<FloatProperty*>(propertyValue))
	{
		float boxWidth = uiRenderer->CalculateStringWidth(*smallFont, "9999.9999");
		float boxHeight = smallFont->lineHeight * 1.1f;
		float boxPadding = (labelHeight - boxHeight) * 0.5f + innerPadding;
		UIFloatBox* button = new UIFloatBox(settingPanel->w - boxWidth - innerPadding, boxPadding, boxWidth, boxHeight, floatProperty->Value(), smallFont, mediumPanelColour, lightPanelColour, textValueColour);

		button->onClick = [floatProperty, button]() {
			floatProperty->SetValue(button->floatValue);
		};

		settingPanel->AddElement(button);
	}
	else if (StringProperty* stringProperty = dynamic_cast<StringProperty*>(propertyValue))
	{
		float boxWidth = uiRenderer->CalculateStringWidth(*smallFont, "lorem ipsum dolor sit amet");
		float boxHeight = smallFont->lineHeight * 1.1f;
		float boxPadding = (labelHeight - boxHeight) * 0.5f + innerPadding;
		UITextBox* button = new UITextBox(settingPanel->w - boxWidth - innerPadding, boxPadding, boxWidth, boxHeight, stringProperty->Value(), smallFont, mediumPanelColour, lightPanelColour, textValueColour);

		button->onClick = [stringProperty, button]() {
			stringProperty->SetValue(button->text);
		};

		settingPanel->AddElement(button);
	}
	else if (Vector3Property* vectorProperty = dynamic_cast<Vector3Property*>(propertyValue))
	{
		float boxWidth = uiRenderer->CalculateStringWidth(*smallFont, "9999.9999");
		float boxHeight = smallFont->lineHeight * 1.1f;
		float boxPadding = (labelHeight - boxHeight) * 0.5f + innerPadding;
		UIFloatBox* buttonX = new UIFloatBox(settingPanel->w - (boxWidth + innerPadding) * 3, boxPadding, boxWidth, boxHeight, vectorProperty->Value().x, smallFont, mediumPanelColour, lightPanelColour, textValueColour);
		UIFloatBox* buttonY = new UIFloatBox(settingPanel->w - (boxWidth + innerPadding) * 2, boxPadding, boxWidth, boxHeight, vectorProperty->Value().y, smallFont, mediumPanelColour, lightPanelColour, textValueColour);
		UIFloatBox* buttonZ = new UIFloatBox(settingPanel->w - (boxWidth + innerPadding) * 1, boxPadding, boxWidth, boxHeight, vectorProperty->Value().z, smallFont, mediumPanelColour, lightPanelColour, textValueColour);

		buttonX->onClick = [vectorProperty, buttonX]() {
			Vector3 value = vectorProperty->Value();
			value.x = buttonX->floatValue;
			vectorProperty->SetValue(value);
		};

		buttonY->onClick = [vectorProperty, buttonY]() {
			Vector3 value = vectorProperty->Value();
			value.x = buttonY->floatValue;
			vectorProperty->SetValue(value);
		};

		buttonZ->onClick = [vectorProperty, buttonZ]() {
			Vector3 value = vectorProperty->Value();
			value.x = buttonZ->floatValue;
			vectorProperty->SetValue(value);
		};

		settingPanel->AddElement(buttonX);
		settingPanel->AddElement(buttonY);
		settingPanel->AddElement(buttonZ);
	}

	settingPanel->h = panelHeight;
	return settingPanel;
}

void SettingsMenu::PopulateBasicSettingsTab()
{
	std::vector<std::string> basicSettingsList = {
		"LeftHanded",
		"SnapTurn",
		"SnapTurnAmount",
		"SmoothTurnAmount",
		"HorizontalVehicleTurnAmount",
		"VerticalVehicleTurnAmount",
		"HandRelativeMovement",
		"EnableWeaponHolsters",
		"ShowCrosshair",
		"ShowRoomCentre"
	};

	// Clear out everything in case this is a refresh after reverting
	basicSettingsPanel->DeleteChildren();

	basicSettingsPages.clear();

	GenerateSettingsTab(basicSettingsList, basicSettingsPanel, basicSettingsPages);

	UIImageButton* prevButton = new UIImageButton(0.0f, 0.0f, settingArrowWidth, basicSettingsPanel->h, prevPageButtonImage);
	prevButton->onClick = [this]() {
		this->PrevPage(basicSettingsPages, basicSettingsPage);
	};
	basicSettingsPanel->AddElement(prevButton);

	UIImageButton* nextButton = new UIImageButton(basicSettingsPanel->w - settingArrowWidth, 0.0f, settingArrowWidth, basicSettingsPanel->h, nextPageButtonImage);
	nextButton->onClick = [this]() {
		this->NextPage(basicSettingsPages, basicSettingsPage);
	};
	basicSettingsPanel->AddElement(nextButton);
}


void SettingsMenu::PopulateAllSettingsTab()
{
	const std::vector<std::string> allSettingsList = Game::instance.config.GetAllSettings();

	// Clear out everything in case this is a refresh after reverting
	allSettingsPanel->DeleteChildren();

	allSettingsPages.clear();

	GenerateSettingsTab(allSettingsList, allSettingsPanel, allSettingsPages);

	UIImageButton* prevButton = new UIImageButton(0.0f, 0.0f, settingArrowWidth, allSettingsPanel->h, prevPageButtonImage);
	prevButton->onClick = [this]() {
		this->PrevPage(allSettingsPages, allSettingsPage);
	};
	allSettingsPanel->AddElement(prevButton);

	UIImageButton* nextButton = new UIImageButton(allSettingsPanel->w - settingArrowWidth, 0.0f, settingArrowWidth, allSettingsPanel->h, nextPageButtonImage);
	nextButton->onClick = [this]() {
		this->NextPage(allSettingsPages, allSettingsPage);
	};
	allSettingsPanel->AddElement(nextButton);
}

void SettingsMenu::PopulateAboutTab(UIPanel* aboutPanel)
{
	std::stringstream aboutStream;

	aboutStream << "Halo Edition: " << (Game::instance.bIsCustom ? "Custom Edition" : "Retail") << std::endl;
	aboutStream << "Halo Version: " << Helpers::GetVersionString() << std::endl;
	aboutStream << "Mod Version: " << MOD_VERSION << std::endl;
	aboutStream << "Chimera: " << (Game::instance.bDetectedChimera ? "" : "NOT ") << "installed" << std::endl;
	aboutStream << "First launch: " << (Game::instance.bLoadedConfig ? "False" : "True") << std::endl;
	aboutStream << "Config file: " << (Game::instance.bSavedConfig ? "Writeable" : "!!!Not Writeable!!!") << std::endl;
	aboutStream << "Headset: " << Game::instance.GetVR()->GetDeviceName() << std::endl;

	aboutPanel->AddElement(new UILabel(10.0f, 10.0f, aboutStream.str(), Game::instance.uiRenderer->GetMediumFont()));
}
