#include "UICheckbox.h"
#include "UIRenderer.h"

UICheckbox::UICheckbox(float x, float y, float size, bool bChecked, DWORD backgroundColour, DWORD checkColour)
{
	this->x = x;
	this->y = y;
	this->w = size;
	this->h = size;
	this->bIsActive = bChecked;
	this->backgroundColour = backgroundColour;
	this->checkColour = checkColour;
	this->bToggle = true;
}

void UICheckbox::Draw(UIRenderer* renderer)
{
	float pX, pY;
	GetScreenPosition(pX, pY);

	renderer->DrawRect(pX, pY, w, w, backgroundColour);

	if (bIsActive)
	{
		float buffer = w * 0.25f;
		float innerSize = w - buffer * 2.0f;
		renderer->DrawRect(pX + buffer, pY + buffer, innerSize, innerSize, checkColour);
	}
}
