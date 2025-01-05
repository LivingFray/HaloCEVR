#include "UIFloatBox.h"
#include <algorithm>
#include <sstream>
#include "../Game.h"

UIFloatBox::UIFloatBox(float x, float y, float w, float h, float value, Font* font, DWORD inactiveColour, DWORD activeColour, DWORD textColour)
	: UITextBox(x, y, w, h, floatToString(value), font, inactiveColour, activeColour, textColour)
{
	this->floatValue = value;
}

void UIFloatBox::ValidateText()
{
	bool bIsNumeric = text.find_first_not_of("0123456789.-") == std::string::npos;
	bool bBadDecimal = std::count_if(text.begin(), text.end(), [](char c) { return c == '.'; }) > 1;
	bool bBadSign = text.find_first_of('-', 1) != std::string::npos;

	if (!bIsNumeric || bBadDecimal || bBadSign)
	{
		text = floatToString(floatValue);
		float textW = Game::instance.uiRenderer->CalculateStringWidth(*font, text.c_str());
		textX = (w - textW) * 0.5f;
	}
	else
	{
		floatValue = static_cast<float>(atof(text.c_str()));
	}
}

std::string UIFloatBox::floatToString(float v) const
{
	std::stringstream ss{};
	ss << v;
	return ss.str();
}
