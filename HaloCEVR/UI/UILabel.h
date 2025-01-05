#pragma once
#include "UIElement.h"
#include "UIRenderer.h"
#include <string>

class UILabel : public UIElement
{
public:
	std::string text;
	struct Font* font;
	DWORD color;

	UILabel(float x, float y, std::string text, struct Font* font, DWORD color = D3DCOLOR_ARGB(255, 255, 255, 255));

	virtual bool Click(float mouseX, float mouseY) override;
	virtual void Draw(class UIRenderer* renderer) override;
};

