#include "UITextBox.h"
#include "../Game.h"

UITextBox::UITextBox(float x, float y, float w, float h, std::string text, Font* font, DWORD inactiveColour, DWORD activeColour, DWORD textColour)
	: UITextButton(x, y, w, h, text, font, inactiveColour, activeColour, textColour)
{
}

bool UITextBox::Click(float mouseX, float mouseY)
{
	const bool bOverlaps = InBounds(mouseX, mouseY, x, y, w, h);

	if (bOverlaps)
	{
		const bool bKeyboardUp = Game::instance.GetVR()->IsKeyboardVisible();
		
		// Don't activate if another box has input
		if (!bIsActive && bKeyboardUp)
		{
			return true;
		}

		bIsActive ^= true;

		if (bIsActive)
		{
			Game::instance.uiRenderer->UpdateActiveButton(this);
			Game::instance.GetVR()->ShowKeyboard(text);
		}
		else
		{
			Game::instance.uiRenderer->UpdateActiveButton(nullptr);
		}
	}

	return bOverlaps;
}

void UITextBox::Draw(UIRenderer* renderer)
{
	if (bIsActive)
	{
		text = Game::instance.GetVR()->GetKeyboardInput();
		float textW = Game::instance.uiRenderer->CalculateStringWidth(*font, text.c_str());
		textX = (w - textW) * 0.5f;
	}

	UITextButton::Draw(renderer);
}

void UITextBox::Deactivate()
{
	bIsActive = false;
	text = Game::instance.GetVR()->GetKeyboardInput();
	Game::instance.GetVR()->HideKeyboard();
	ValidateText();

	if (onClick)
	{
		onClick();
	}

	Logger::log << "Deactivated" << std::endl;
}

void UITextBox::ValidateText()
{
}
