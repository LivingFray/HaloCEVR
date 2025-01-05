#include "UITextButton.h"
#include "../Game.h"

UITextButton::UITextButton(float x, float y, float w, float h, std::string text, Font* font, DWORD inactiveColour, DWORD activeColour, DWORD textColour)
{
	this->x = x;
	this->y = y;
	this->w = w;
	this->h = h;
	this->text = text;
	this->font = font;
	this->inactiveColour = inactiveColour;
	this->activeColour = activeColour;
	this->textColour = textColour;

	float textW = Game::instance.uiRenderer->CalculateStringWidth(*font, text.c_str());

	textX = (w - textW) * 0.5f;
	textY = (h - font->lineHeight) * 0.5f;
}

void UITextButton::Draw(UIRenderer* renderer)
{
	float pX, pY;
	GetScreenPosition(pX, pY);
	renderer->DrawRect(pX, pY, w, h, bIsActive ? activeColour : inactiveColour);
	renderer->Print(*font, text.c_str(), pX + textX, pY + textY, textColour);
}
