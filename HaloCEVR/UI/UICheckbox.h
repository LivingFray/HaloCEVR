#pragma once
#include <d3d9.h>
#include "UIButton.h"
class UICheckbox : public UIButton
{
public:
	DWORD backgroundColour;
	DWORD checkColour;

	UICheckbox(float x, float y, float size, bool bChecked, DWORD backgroundColour, DWORD checkColour);

	virtual void Draw(class UIRenderer* renderer) override;
};

