#include "UIElement.h"

void UIElement::GetScreenPosition(float& outX, float& outY) const
{
	if (!parent)
	{
		outX = x;
		outY = y;
	}
	else
	{
		float parentX, parentY;
		parent->GetScreenPosition(parentX, parentY);

		outX = parentX + x;
		outY = parentY + y;
	}
}

bool UIElement::InBounds(float posX, float posY, float boundsX, float boundsY, float width, float height)
{
	const bool bInX = posX >= boundsX && posX <= boundsX + width;
	const bool bInY = posY >= boundsY && posY <= boundsY + height;

    return bInX && bInY;
}
