#include "UIIntBox.h"
#include <algorithm>
#include <sstream>
#include "../Game.h"

UIIntBox::UIIntBox(float x, float y, float w, float h, int value, Font* font, DWORD inactiveColour, DWORD activeColour, DWORD textColour)
	: UITextBox(x, y, w, h, std::to_string(value), font, inactiveColour, activeColour, textColour)
{
	this->intValue = value;
}

void UIIntBox::ValidateText()
{
	bool bIsNumeric = text.find_first_not_of("0123456789-") == std::string::npos;
	bool bBadSign = text.find_first_of('-', 1) != std::string::npos;

	if (!bIsNumeric || bBadSign)
	{
		text = std::to_string(intValue);
		float textW = Game::instance.uiRenderer->CalculateStringWidth(*font, text.c_str());
		textX = (w - textW) * 0.5f;
	}
	else
	{
		intValue = atoi(text.c_str());
	}
}
