#include "UIImageButton.h"
#include "UIRenderer.h"

UIImageButton::UIImageButton(float x, float y, float w, float h, IDirect3DTexture9* image)
{
	this->x = x;
	this->y = y;
	this->w = w;
	this->h = h;
	this->image = image;
}

void UIImageButton::Draw(UIRenderer* renderer)
{
	float pX, pY;
	GetScreenPosition(pX, pY);
	renderer->DrawImage(pX, pY, w, h, image);
}
