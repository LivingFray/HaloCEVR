#pragma once
#include "UIElement.h"
#include <vector>
class UITabPanel : public UIElement
{
public:
	float w;
	float h;

	UITabPanel(float x, float y, float w, float h);

	void AddTab(class UIButton* button, class UIPanel* panel);

	virtual bool Click(float mouseX, float mouseY) override;
	virtual void Draw(class UIRenderer* renderer) override;

	void SetActiveTab(size_t index);

protected:

	int activeTab = 0;

	float nextButtonPos;

	std::vector<class UIButton*> buttons;
	std::vector<class UIPanel*> panels;
};

