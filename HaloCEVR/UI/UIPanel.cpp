#include "UIPanel.h"

UIPanel::UIPanel(float x, float y, float w, float h, DWORD color, bool bDrawBackground)
{
	this->x = x;
	this->y = y;
	this->w = w;
	this->h = h;
	this->color = color;
	this->bDrawBackground = bDrawBackground;
}

bool UIPanel::Click(float mouseX, float mouseY)
{
	mouseX -= x;
	mouseY -= y;
	for (UIElement* child : childElements)
	{
		if (child->bVisible)
		{
			if (child->Click(mouseX, mouseY))
			{
				return true;
			}
		}
	}

	return false;
}

void UIPanel::Draw(UIRenderer* renderer)
{
	if (bDrawBackground)
	{
		float pX, pY;
		GetScreenPosition(pX, pY);
		renderer->DrawRect(pX, pY, w, h, color);
	}

	for (UIElement* child : childElements)
	{
		if (child->bVisible)
		{
			child->Draw(renderer);
		}
	}
}

void UIPanel::AddElement(UIElement* element)
{
	childElements.push_back(element);
	element->parent = this;
}

void UIPanel::RemoveElement(UIElement* element)
{
	auto it = std::find(childElements.begin(), childElements.end(), element); 
	if (it != childElements.end()) 
	{
		childElements.erase(it);
	}
}

void UIPanel::DeleteChildren()
{
	for (UIElement* child : childElements)
	{
		if (UIPanel* childPanel = dynamic_cast<UIPanel*>(child))
		{
			childPanel->DeleteChildren();
		}
		delete child;
	}
	childElements.clear();
}
