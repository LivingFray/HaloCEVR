#pragma once
#include "UITextBox.h"
class UIFloatBox : public UITextBox
{
public:

	UIFloatBox(float x, float y, float w, float h, float value, Font* font, DWORD inactiveColour, DWORD activeColour, DWORD textColour);

	float floatValue;
protected:
	virtual void ValidateText() override;
	std::string floatToString(float v) const;
};

