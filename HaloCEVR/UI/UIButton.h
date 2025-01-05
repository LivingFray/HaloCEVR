#pragma once
#include "UIElement.h"
#include <functional>
class UIButton : public UIElement
{
public:
	float w;
	float h;

	bool bIsActive = false;
	bool bToggle = false;

	std::function<void (void)> onClick;

	virtual bool Click(float mouseX, float mouseY) override;
	virtual void Deactivate();
};

