#include "UIRenderer.h"

#define STB_TRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#include "../../ThirdParty/stb/stb_truetype.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../../ThirdParty/stb/stb_image.h"
#include <stdio.h>
#include <d3d9.h>
#include "UIElement.h"
#include "../Game.h"
#include "UIButton.h"


void UIRenderer::Init(IDirect3DDevice9* inDevice)
{
	device = inDevice;

	InitFont("VR/Fonts/normal.ttf", 56.0f, fontTitle);
	InitFont("VR/Fonts/normal.ttf", 32.0f, fontLarge);
	InitFont("VR/Fonts/normal.ttf", 28.0f, fontMedium);
	InitFont("VR/Fonts/normal.ttf", 18.0f, fontSmall);
	InitFont("VR/Fonts/italics.ttf", 18.0f, fontDesc);

	// Assume the UI is rendering on a 640x480 canvas and just scale everything accordingly
	widthScale = Game::instance.c_UIOverlayWidth->Value() / 640.0f;
	heightScale = Game::instance.c_UIOverlayHeight->Value() / 480.0f;
}

bool UIRenderer::InitFont(const char* fontPath, float fontSize, Font& outFont)
{
	unsigned char* ttf_buffer = new unsigned char[1 << 20];
	unsigned char* temp_bitmap = new unsigned char[512 * 512];

	FILE* fontFile = fopen(fontPath, "rb");
	if (!fontFile)
	{
		return false;
	}

	if (fread(ttf_buffer, 1, 1 << 20, fontFile) == 0)
	{
		return false;
	}

	int rows = stbtt_BakeFontBitmap(ttf_buffer, 0, fontSize, temp_bitmap, 512, 512, 32, 96, outFont.cdata);

	if (rows <= 0)
	{
		delete[] ttf_buffer;
		delete[] temp_bitmap;

		return false;
	}

	device->CreateTexture(512, 512, 0, D3DUSAGE_DYNAMIC, D3DFMT_A8, D3DPOOL_DEFAULT, &outFont.fontBitmap, nullptr);
	D3DLOCKED_RECT rect;
	outFont.fontBitmap->LockRect(0, &rect, NULL, 0);
	memcpy(rect.pBits, temp_bitmap, 512 * 512);
	outFont.fontBitmap->UnlockRect(0);

	stbtt_fontinfo font{};
	stbtt_InitFont(&font, ttf_buffer, stbtt_GetFontOffsetForIndex(ttf_buffer, 0));

	int ascent, descent, lineGap;
	stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
	float scale = stbtt_ScaleForPixelHeight(&font, fontSize);

	outFont.offset = ascent * scale;
	outFont.lineHeight = (ascent - descent) * scale;

	delete[] ttf_buffer;
	delete[] temp_bitmap;

	return true;
}

IDirect3DTexture9* UIRenderer::LoadTexture(const char* imagePath)
{
	int width, height, n;
	unsigned char* data = stbi_load(imagePath, &width, &height, &n, 4);

	if (!data)
	{
		return nullptr;
	}

	IDirect3DTexture9* outTexture;

	HRESULT result = device->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &outTexture, NULL);

	if (FAILED(result))
	{
		Logger::log << "[UI] Can't load texture " << imagePath << std::endl;
		stbi_image_free(data);
		return nullptr;
	}


	D3DLOCKED_RECT rect;
	if (SUCCEEDED(outTexture->LockRect(0, &rect, NULL, D3DLOCK_DISCARD)))
	{
		BYTE* dest = (BYTE*)rect.pBits;
		const BYTE* src = data;
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x)
			{
				// src is stored as rgba, texture is stored in bgra
				unsigned char pixel[4]{
					src[2], // b
					src[1], // g
					src[0], // r
					src[3], // a
				};
				memcpy(&dest[rect.Pitch * y + 4 * x], &pixel, 4);

				src += 4;
			}
		}
		outTexture->UnlockRect(0);
	}

	stbi_image_free(data);

	return outTexture;
}

void UIRenderer::Shutdown()
{
}

void UIRenderer::MoveCursor(float x, float y)
{
	mouseX = x;
	mouseY = y;

	// todo: on element hover events
}

void UIRenderer::Click()
{
	for (UIElement* element : uiElements)
	{
		if (element->bVisible)
		{
			bool bConsumed = element->Click(mouseX, mouseY);

			if (bConsumed)
			{
				return;
			}
		}
	}

	// Nothing consumed input, deselect whatever button was last activated
	Game::instance.uiRenderer->UpdateActiveButton(nullptr);
}

void UIRenderer::AddElement(UIElement* element)
{
	uiElements.push_back(element);
}

void UIRenderer::Render()
{
	LPDIRECT3DSTATEBLOCK9 stateBlock = NULL;
	device->CreateStateBlock(D3DSBT_ALL, &stateBlock);

	for (UIElement* element : uiElements)
	{
		if (element->bVisible)
		{
			element->Draw(this);
		}
	}

#if EMULATE_VR
	DrawRect(mouseX - 2, mouseY - 2, 4.0f, 4.0f, D3DCOLOR_ARGB(200, 255, 255, 0));
#endif

	stateBlock->Apply();
	stateBlock->Release();
}

void scaled_GetBakedQuad(const stbtt_bakedchar* chardata, int pw, int ph, int char_index, float* xpos, float* ypos, stbtt_aligned_quad* q, float sX, float sY, int opengl_fillrule)
{
	float d3d_bias = opengl_fillrule ? 0 : -0.5f;
	float ipw = 1.0f / pw, iph = 1.0f / ph;
	const stbtt_bakedchar* b = chardata + char_index;
	float x = (*xpos + b->xoff * sX) + 0.5f;
	float y = (*ypos + b->yoff * sY) + 0.5f;

	q->x0 = STBTT_ifloor(x) + d3d_bias;
	q->y0 = STBTT_ifloor(y) + d3d_bias;
	q->x1 = STBTT_ifloor(x + (b->x1 - b->x0) * sX) + d3d_bias;
	q->y1 = STBTT_ifloor(y + (b->y1 - b->y0) * sY) + d3d_bias;

	q->s0 = b->x0 * ipw;
	q->t0 = b->y0 * iph;
	q->s1 = b->x1 * ipw;
	q->t1 = b->y1 * iph;

	*xpos += b->xadvance * sX;
}

void UIRenderer::DrawRect(float x, float y, float w, float h, DWORD color) const
{
	device->SetTexture(0, NULL);
	device->SetRenderState(D3DRS_LIGHTING, false);
	device->SetRenderState(D3DRS_ZENABLE, false);

	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);

	device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	device->SetRenderState(D3DRS_SRGBWRITEENABLE, TRUE);

	device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);

	x *= widthScale;
	y *= heightScale;
	w *= widthScale;
	h *= heightScale;

	VertexData2D vertices[4] = {
		{x, y, 0.0f, 1.0f, color},
		{x, y + h, 0.0f, 1.0f, color},
		{x + w, y, 0.0f, 1.0f, color},
		{x + w, y + h, 0.0f, 1.0f, color},
	};
	device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(VertexData2D));
}

void UIRenderer::DrawImage(float x, float y, float w, float h, IDirect3DTexture9* image) const
{
	device->SetTexture(0, image);
	device->SetRenderState(D3DRS_LIGHTING, false);
	device->SetRenderState(D3DRS_ZENABLE, false);

	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);

	device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);

	device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);

	x *= widthScale;
	y *= heightScale;
	w *= widthScale;
	h *= heightScale;

	DWORD color = D3DCOLOR_ARGB(255, 255, 255, 255);

	VertexDataTex2D vertices[4] = {
		{x, y, 0.0f, 1.0f, color, 0.0f, 0.0f},
		{x, y + h, 0.0f, 1.0f, color, 0.0f, 1.0f},
		{x + w, y, 0.0f, 1.0f, color, 1.0f, 0.0f},
		{x + w, y + h, 0.0f, 1.0f, color, 1.0f, 1.0f},
	};
	device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(VertexDataTex2D));
}


// Helper function to apply inverse gamma correction
D3DCOLOR ApplyInverseGammaCorrection(D3DCOLOR color, float gamma) {
	// Extract the RGBA components from the D3DCOLOR
	BYTE r = (color >> 16) & 0xFF;
	BYTE g = (color >> 8) & 0xFF;
	BYTE b = color & 0xFF;
	BYTE a = (color >> 24) & 0xFF;

	// Normalize the color components to [0, 1] range
	float fr = r / 255.0f;
	float fg = g / 255.0f;
	float fb = b / 255.0f;

	// Apply inverse gamma correction
	float inverseGamma = 1.0f / gamma;
	fr = pow(fr, inverseGamma);
	fg = pow(fg, inverseGamma);
	fb = pow(fb, inverseGamma);

	// Convert the corrected color back to [0, 255] range
	r = static_cast<BYTE>(fr * 255.0f);
	g = static_cast<BYTE>(fg * 255.0f);
	b = static_cast<BYTE>(fb * 255.0f);

	// Recombine the RGBA components back into a D3DCOLOR
	return D3DCOLOR_ARGB(a, r, g, b);
}

void UIRenderer::Print(const Font& font, const char* text, float x, float y, DWORD color) const
{
	// There's some weird post process gamma correction going on that messes with the text colour
	color = ApplyInverseGammaCorrection(color, 2.2f);

	device->SetTexture(0, font.fontBitmap);
	device->SetRenderState(D3DRS_LIGHTING, false);
	device->SetRenderState(D3DRS_ZENABLE, false);

	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);

	device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);

	device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);

	x *= widthScale;
	y *= heightScale;

	float initX = x;

	y += font.offset * heightScale;

	for (int i = 0; text[i] != '\0'; ++i) {
		char currentChar = text[i];
		if (currentChar == '\n')
		{
			x = initX;
			y += font.lineHeight * heightScale;
			continue;
		}
		if (currentChar < 32 || currentChar > 127)
		{
			currentChar = '?';
		}

		stbtt_aligned_quad q;
		scaled_GetBakedQuad(font.cdata, 512, 512, currentChar - 32, &x, &y, &q, widthScale, heightScale, 0);
		VertexDataTex2D vertices[4] = {
			{q.x0, q.y0, 0.0f, 1.0f, color, q.s0, q.t0},
			{q.x0, q.y1, 0.0f, 1.0f, color, q.s0, q.t1},
			{q.x1, q.y0, 0.0f, 1.0f, color, q.s1, q.t0},
			{q.x1, q.y1, 0.0f, 1.0f, color, q.s1, q.t1},
		};
		device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(VertexDataTex2D));
	}
}

float UIRenderer::CalculateWrappedString(const Font& font, const char* text, float w, std::string& wrappedString) const
{
	// Go word by word to get the wrap point and insert newlines, tracking the number of lines for the output
	int numLines = 1;

	float lineWidth = 0.0f;

	int lineStart = 0;
	int lastGoodWord = -1;

	for (int i = 0; text[i] != '\0'; ++i) {
		char currentChar = text[i];
		if (currentChar == '\n')
		{
			lineWidth = 0.0f;
			numLines++;
			lastGoodWord = -1;
			lineStart = i + 1;
			wrappedString += currentChar;
			continue;
		}
		if (currentChar < 32 || currentChar > 127)
		{
			currentChar = '?';
		}

		lineWidth += font.cdata[currentChar - 32].xadvance;

		if (lineWidth > w && lastGoodWord > -1)
		{
			for (int c = lineStart; c < lastGoodWord; c++)
			{
				wrappedString += text[c];
			}

			wrappedString += '\n';
			numLines++;
			lineWidth = 0.0f;

			lineStart = lastGoodWord;
			lastGoodWord = -1;
			i = lineStart - 1;
		}
		else
		{
			if (currentChar == ' ')
			{
				lastGoodWord = i + 1;
			}
		}
	}

	for (int c = lineStart; text[c] != '\0'; c++)
	{
		wrappedString += text[c];
	}

	return numLines * font.lineHeight;
}

float UIRenderer::CalculateStringWidth(const Font& font, const char* text) const
{
	float maxLineWidth = 0.0f;
	float lineWidth = 0.0f;

	for (int i = 0; text[i] != '\0'; ++i) {
		char currentChar = text[i];
		if (currentChar == '\n')
		{
			if (lineWidth > maxLineWidth)
			{
				maxLineWidth = lineWidth;
			}

			lineWidth = 0.0f;
			continue;
		}
		if (currentChar < 32 || currentChar > 127)
		{
			currentChar = '?';
		}

		lineWidth += font.cdata[currentChar - 32].xadvance;
	}

	if (maxLineWidth > lineWidth)
	{
		lineWidth = maxLineWidth;
	}

	return lineWidth;
}

void UIRenderer::UpdateActiveButton(UIButton* button)
{
	if (activeButton)
	{
		activeButton->Deactivate();
	}

	activeButton = button;
}
