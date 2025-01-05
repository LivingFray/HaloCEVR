#pragma once
#include "UIButton.h"
#include <string>
#include "UIRenderer.h"
class UITextButton : public UIButton
{
public:
	std::string text;
	struct Font* font;
	DWORD inactiveColour;
	DWORD activeColour;
	DWORD textColour;

	UITextButton(float x, float y, float w, float h, std::string text, Font* font, DWORD inactiveColour, DWORD activeColour, DWORD textColour);

	virtual void Draw(class UIRenderer* renderer) override;

protected:

	float textX;
	float textY;
};

