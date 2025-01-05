#pragma once
#include <d3d9.h>
#include "UIButton.h"
class UIImageButton : public UIButton
{
public:
	IDirect3DTexture9* image;

	UIImageButton(float x, float y, float w, float h, IDirect3DTexture9* image);

	virtual void Draw(class UIRenderer* renderer) override;
};

