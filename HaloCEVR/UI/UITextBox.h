#pragma once
#include "UITextButton.h"
class UITextBox : public UITextButton
{
public:

	UITextBox(float x, float y, float w, float h, std::string text, Font* font, DWORD inactiveColour, DWORD activeColour, DWORD textColour);

	virtual bool Click(float mouseX, float mouseY) override;
	virtual void Draw(class UIRenderer* renderer) override;
	virtual void Deactivate() override;

protected:
	virtual void ValidateText();
};

