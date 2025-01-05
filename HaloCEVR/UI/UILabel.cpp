#include "UILabel.h"

UILabel::UILabel(float x, float y, std::string text, Font* font, DWORD color)
{
	this->x = x;
	this->y = y;
	this->text = text;
	this->font = font;
	this->color = color;
}

bool UILabel::Click(float mouseX, float mouseY)
{
	return false;
}

void UILabel::Draw(UIRenderer* renderer)
{
	float pX, pY;
	GetScreenPosition(pX, pY);
	renderer->Print(*font, text.c_str(), pX, pY, color);
}
