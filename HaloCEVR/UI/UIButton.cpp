#include "UIButton.h"
#include "../Game.h"

bool UIButton::Click(float mouseX, float mouseY)
{
	const bool bOverlaps = InBounds(mouseX, mouseY, x, y, w, h);

	if (bOverlaps)
	{
		if (bToggle)
		{
			bIsActive ^= true;

			if (bIsActive)
			{
				Game::instance.uiRenderer->UpdateActiveButton(this);
			}
			else
			{
				Game::instance.uiRenderer->UpdateActiveButton(nullptr);
			}
		}
		else
		{
			Game::instance.uiRenderer->UpdateActiveButton(nullptr);
		}

		if (onClick)
		{
			onClick();
		}
	}

    return bOverlaps;
}

void UIButton::Deactivate()
{
	if (!bToggle)
	{
		bIsActive = false;
	}
}
