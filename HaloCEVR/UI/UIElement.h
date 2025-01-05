#pragma once
class UIElement
{
public:
	float x;
	float y;
	bool bVisible = true;
	UIElement* parent;

	virtual bool Click(float mouseX, float mouseY) = 0;
	virtual void Draw(class UIRenderer* renderer) = 0;

	void GetScreenPosition(float& outX, float& outY) const;

protected:

	// Helper function, returns true if Pos is within the bounding box defined
	// by the provided x/y coordinates and width/height
	static bool InBounds(float posX, float posY, float boundsX, float boundsY, float width, float height);
};

