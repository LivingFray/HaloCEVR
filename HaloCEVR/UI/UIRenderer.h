#pragma once
#include "../../ThirdParty/stb/stb_truetype.h"
#include <d3d9.h>
#include <vector>
#include <string>

struct Font
{
	IDirect3DTexture9* fontBitmap;
	stbtt_bakedchar cdata[96];
	float offset;
	float fontSize;
	float lineHeight;
};

class UIRenderer
{
public:
	void Init(IDirect3DDevice9* inDevice);
	bool InitFont(const char* fontPath, float fontSize, Font& outFont);
	IDirect3DTexture9* LoadTexture(const char* imagePath);
	void Shutdown();

	void MoveCursor(float x, float y);
	void Click();

	void AddElement(class UIElement* element);

	void Render();

	void DrawRect(float x, float y, float w, float h, DWORD color) const;
	void DrawImage(float x, float y, float w, float h, IDirect3DTexture9* image) const;
	void Print(const Font& font, const char* text, float x, float y, DWORD color = D3DCOLOR_ARGB(255, 255, 255, 255)) const;

	float CalculateWrappedString(const Font& font, const char* text, float w, std::string& wrappedString) const;
	float CalculateStringWidth(const Font& font, const char* text) const;

	void UpdateActiveButton(class UIButton* button);

	Font* GetTitleFont() { return &fontTitle; }
	Font* GetLargeFont() { return &fontLarge; }
	Font* GetMediumFont() { return &fontMedium; }
	Font* GetSmallFont() { return &fontSmall; }
	Font* GetDescriptionFont() { return &fontDesc; }

protected:

	// All UI elements to be rendered, will render in the order elements were added
	std::vector<class UIElement*> uiElements;

	class UIButton* activeButton = nullptr;

	float mouseX = 0.0f;
	float mouseY = 0.0f;

	IDirect3DDevice9* device;

	Font fontTitle;
	Font fontLarge;
	Font fontMedium;
	Font fontSmall;
	Font fontDesc;

	float widthScale, heightScale;

	struct VertexData2D
	{
		float x, y, z, rhw;
		DWORD color;
	};

	struct VertexDataTex2D
	{
		float x, y, z, rhw;
		DWORD color;
		float u, v;
	};
};

