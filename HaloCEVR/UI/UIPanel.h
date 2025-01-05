#pragma once
#include "UIElement.h"
#include "UIRenderer.h"
class UIPanel : public UIElement
{
public:
	float w;
	float h;
	bool bDrawBackground = true;
	DWORD color;

	UIPanel(float x, float y, float w, float h, DWORD color, bool bDrawBackground = true);

	virtual bool Click(float mouseX, float mouseY) override;
	virtual void Draw(UIRenderer* renderer) override;

	void AddElement(class UIElement* element);
	void RemoveElement(class UIElement* element);
	void DeleteChildren();

protected:

	std::vector<class UIElement*> childElements;
};

