#include "UITabPanel.h"
#include "UIButton.h"
#include "UIPanel.h"
#include "../Logger.h"

UITabPanel::UITabPanel(float x, float y, float w, float h)
{
	this->x = x;
	this->y = y;
	this->w = w;
	this->h = h;

	this->nextButtonPos = 0.0f;
}

void UITabPanel::AddTab(UIButton* button, UIPanel* panel)
{
	button->parent = this;
	button->x = nextButtonPos;
	button->bToggle = true;
	nextButtonPos += button->w;
	
	size_t index = buttons.size();

	button->onClick = [index,this]() {
		this->SetActiveTab(index);
	};

	// Default to first panel being active
	panel->bVisible = buttons.empty();
	button->bIsActive = buttons.empty();

	buttons.push_back(button);
	panels.push_back(panel);
}

bool UITabPanel::Click(float mouseX, float mouseY)
{
	mouseX -= x;
	mouseY -= y;

	for (class UIButton* button : buttons)
	{
		if (button->bVisible)
		{
			if (button->Click(mouseX, mouseY))
			{
				return true;
			}
		}
	}

	return false;
}

void UITabPanel::Draw(UIRenderer* renderer)
{
	for (UIButton* button : buttons)
	{
		if (button->bVisible)
		{
			button->Draw(renderer);
		}
	}
}

void UITabPanel::SetActiveTab(size_t index)
{
	for (size_t i = 0; i < panels.size(); i++)
	{
		if (index == i)
		{
			panels[i]->bVisible = true;
			buttons[i]->bIsActive = true;
		}
		else
		{
			panels[i]->bVisible = false;
			buttons[i]->bIsActive = false;
		}
	}
}
