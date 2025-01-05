#pragma once
#include "UITextBox.h"
class UIIntBox : public UITextBox
{
public:

	UIIntBox(float x, float y, float w, float h, int value, Font* font, DWORD inactiveColour, DWORD activeColour, DWORD textColour);

	int intValue;
protected:
	virtual void ValidateText() override;
};

