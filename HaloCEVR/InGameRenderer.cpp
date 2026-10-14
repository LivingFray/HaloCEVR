#include "InGameRenderer.h"
#include "Immersion/RenderProjection.h"
#include "Helpers/Renderer.h"
#include "Helpers/Maths.h"
#include "Game.h"
#include "Hooking/Hooks.h"
#include "UI/UIRenderer.h"

//================================//Debug API//================================//

void InGameRenderer::DrawLine2D(const Vector2& start, const Vector2& end, D3DCOLOR color)
{
	VertexData2D startData
	{
		start.x,
		start.y,
		0.0f,
		1.0f,
		color

	};
	lines2D.push_back(startData);

	VertexData2D endData
	{
		end.x,
		end.y,
		0.0f,
		1.0f,
		color

	};
	lines2D.push_back(endData);
}

void InGameRenderer::DrawInvertedShape2D(const Vector2& centre, const Vector2& innerSize, const Vector2& size, int sides, float radius, D3DCOLOR color)
{
	// Draw quarter shape for each corner
	int qSides = sides / 4;
	const float increment = -6.28318530718f / sides;
	constexpr float quarterCircle = 3.14159265359f / 2;

	VertexData2D vertex{
		0.0f,
		0.0f,
		0.0f,
		1.0f,
		color
	};

	auto DrawQuadrantInner = [&](Polygon2D& polygon, Vector2 quadCentre, float offset = 0.0f)
		{
			for (int i = 0; i <= qSides; i++)
			{
				float angle = increment * i - offset;
				Vector2 newPoint = quadCentre + Vector2(sin(angle), -cos(angle)) * radius;

				vertex.x = newPoint.x;
				vertex.y = newPoint.y;
				polygon.vertices.push_back(vertex);
			}
		};

	auto DrawQuadrant = [&](Vector2 corner, Vector2 quadCentre, Vector2 startEdge, Vector2 endEdge, int i)
		{
			Polygon2D polygon;
			// Corner vertex
			vertex.x = corner.x;
			vertex.y = corner.y;
			polygon.vertices.push_back(vertex);
			// Vertex on edge, near quadrant start
			vertex.x = startEdge.x;
			vertex.y = startEdge.y;
			polygon.vertices.push_back(vertex);

			// Quadrant
			DrawQuadrantInner(polygon, quadCentre, quarterCircle * i);

			// Vertex on edge, near quadrant end
			vertex.x = endEdge.x;
			vertex.y = endEdge.y;
			polygon.vertices.push_back(vertex);

			// Submit quarter
			polygons2D.push_back(polygon);
		};

	auto DrawConnector = [&](Vector2 topLeft, Vector2 bottomRight)
		{
			Polygon2D polygon;

			vertex.x = topLeft.x;
			vertex.y = topLeft.y;
			polygon.vertices.push_back(vertex);

			vertex.x = bottomRight.x;
			vertex.y = topLeft.y;
			polygon.vertices.push_back(vertex);

			vertex.x = bottomRight.x;
			vertex.y = bottomRight.y;
			polygon.vertices.push_back(vertex);

			vertex.x = topLeft.x;
			vertex.y = bottomRight.y;
			polygon.vertices.push_back(vertex);

			polygons2D.push_back(polygon);
		};

	Vector2 tempCentre = centre;
	Vector2 halfInner = innerSize / 2;
	Vector2 topLeft, bottomRight;

	tempCentre = centre + Vector2(-halfInner.x, -halfInner.y);
	bottomRight = Vector2(tempCentre.x, 0.0f);
	DrawQuadrant(Vector2(0.0f, 0.0f), tempCentre, Vector2(tempCentre.x, 0.0f), Vector2(0.0f, tempCentre.y), 0);
	topLeft = Vector2(0.0f, tempCentre.y);

	tempCentre = centre + Vector2(-halfInner.x, +halfInner.y);
	DrawConnector(topLeft, Vector2(tempCentre.x - radius, tempCentre.y));
	topLeft = Vector2(tempCentre.x, tempCentre.y + radius);
	DrawQuadrant(Vector2(0.0f, size.y), tempCentre, Vector2(0.0f, tempCentre.y), Vector2(tempCentre.x, size.y), 1);

	tempCentre = centre + Vector2(+halfInner.x, +halfInner.y);
	DrawConnector(topLeft, Vector2(tempCentre.x, size.y));
	topLeft = Vector2(size.x, tempCentre.y);
	DrawQuadrant(size, tempCentre, Vector2(tempCentre.x, size.y), Vector2(size.x, tempCentre.y), 2);

	tempCentre = centre + Vector2(+halfInner.x, -halfInner.y);
	DrawQuadrant(Vector2(size.x, 0.0f), tempCentre, Vector2(size.x, tempCentre.y), Vector2(tempCentre.x, 0.0f), 3);
	DrawConnector(topLeft, Vector2(tempCentre.x + radius, tempCentre.y));
	DrawConnector(Vector2(tempCentre.x, tempCentre.y - radius), bottomRight);
}

void InGameRenderer::DrawLine3D(const Vector3& start, const Vector3& end, D3DCOLOR color, bool bRespectDepth, float thickness)
{
	VertexData3D startData
	{
		start.x,
		start.y,
		start.z,
		color
	};

	if (bRespectDepth)
	{
		depthLines3D.push_back(startData);
	}
	else
	{
		lines3D.push_back(startData);
	}

	VertexData3D endData
	{
		end.x,
		end.y,
		end.z,
		color
	};

	if (bRespectDepth)
	{
		depthLines3D.push_back(endData);
	}
	else
	{
		lines3D.push_back(endData);
	}
}

void InGameRenderer::DrawPolygon(const Vector3& centre, const Vector3& facing, const Vector3& upVector, int sides, float radius, D3DCOLOR color, bool bRespectDepth, float angleAmount)
{
	Polygon newPoly{};

	VertexData3D vertex{};

	int alpha = ((color >> 24) & 255);

	if (alpha == 0)
	{
		color |= D3DCOLOR_ARGB(255, 0, 0, 0);
		newPoly.bIsStencil = true;
	}

	vertex.color = color;
	vertex.x = centre.x;
	vertex.y = centre.y;
	vertex.z = centre.z;

	newPoly.vertices.push_back(vertex);

	const Vector3 rightVector = facing.cross(upVector);

	Vector3 firstPoint = centre + upVector * radius;

	vertex.x = firstPoint.x;
	vertex.y = firstPoint.y;
	vertex.z = firstPoint.z;
	newPoly.vertices.push_back(vertex);

	const float increment = angleAmount * -6.28318530718f / sides;

	for (int i = 1; i < sides; i++)
	{
		float angle = increment * i;
		Vector3 newPoint = centre + (sin(angle) * rightVector + cos(angle) * upVector) * radius;

		vertex.x = newPoint.x;
		vertex.y = newPoint.y;
		vertex.z = newPoint.z;
		newPoly.vertices.push_back(vertex);
	}

	if ((1.0f - angleAmount) < 1e-8f)
	{
		vertex.x = firstPoint.x;
		vertex.y = firstPoint.y;
		vertex.z = firstPoint.z;
		newPoly.vertices.push_back(vertex);
	}
	else
	{
		float angle = increment * sides;
		Vector3 newPoint = centre + (sin(angle) * rightVector + cos(angle) * upVector) * radius;

		vertex.x = newPoint.x;
		vertex.y = newPoint.y;
		vertex.z = newPoint.z;
		newPoly.vertices.push_back(vertex);
	}

	if (bRespectDepth)
	{
		depthPolygons.push_back(newPoly);
	}
	else
	{
		polygons.push_back(newPoly);
	}
}

void InGameRenderer::DrawCoordinate(const Vector3& pos, const Matrix3& rot, float size, bool bRespectDepth)
{
	Vector3 up = pos + rot * Vector3(0.0f, 0.0f, size);
	Vector3 forward = pos + rot * Vector3(0.0f, size, 0.0f);
	Vector3 left = pos + rot * Vector3(size, 0.0f, 0.0f);

	DrawLine3D(pos, up, D3DCOLOR_XRGB(0, 0, 255), bRespectDepth, 0.01f);
	DrawLine3D(pos, forward, D3DCOLOR_XRGB(0, 255, 0), bRespectDepth, 0.01f);
	DrawLine3D(pos, left, D3DCOLOR_XRGB(255, 0, 0), bRespectDepth, 0.01f);
}

void InGameRenderer::DrawRenderTarget(IDirect3DTexture9* renderTarget, const Vector3& pos, const Matrix3& rot, const Vector2& size, bool bRespectDepth, bool bRespectStencil)
{
	RenderTarget rtData
	{
		pos,
		rot,
		size * 0.5f,
		renderTarget,
		bRespectDepth,
		bRespectStencil
	};

	renderTargets.push_back(rtData);
}

//================================//Core Functions//================================//

void InGameRenderer::ExtractMatrices(Renderer* playerRenderer)
{
	CameraRenderMatrices& cameraMatrices = *Helpers::GetActiveCameraMatrices();
	Game::instance.SetViewportScale(&cameraMatrices.viewport);

	Hooks::SetCameraMatrices(&cameraMatrices.viewport, &playerRenderer->frustum2, &cameraMatrices, true);


	view._11 = cameraMatrices.viewMatrix.rotation[0];
	view._12 = cameraMatrices.viewMatrix.rotation[1];
	view._13 = cameraMatrices.viewMatrix.rotation[2];
	view._14 = 0.0;
	view._21 = cameraMatrices.viewMatrix.rotation[3];
	view._22 = cameraMatrices.viewMatrix.rotation[4];
	view._23 = cameraMatrices.viewMatrix.rotation[5];
	view._24 = 0.0;
	view._31 = cameraMatrices.viewMatrix.rotation[6];
	view._32 = cameraMatrices.viewMatrix.rotation[7];
	view._33 = cameraMatrices.viewMatrix.rotation[8];
	view._34 = 0.0;
	view._41 = cameraMatrices.viewMatrix.translation.x;
	view._43 = cameraMatrices.viewMatrix.translation.z;
	view._42 = cameraMatrices.viewMatrix.translation.y;
	view._44 = 1.0;
	world._43 = 0.0;
	world._42 = 0.0;
	world._41 = 0.0;
	world._34 = 0.0;
	world._32 = 0.0;
	world._31 = 0.0;
	world._24 = 0.0;
	world._23 = 0.0;
	world._21 = 0.0;
	world._14 = 0.0;
	world._13 = 0.0;
	world._12 = 0.0;
	world._44 = 1.0;
	world._33 = 1.0;
	world._22 = 1.0;
	world._11 = 1.0;
	projection = cameraMatrices.projectionMatrix;
    // Procedural held pieces and the supply outline share the native
    // first-person model pass's depth range, not its world/scenery projection.
    meshProjection = Immersion::FirstPersonDepth(projection);

	// Extract the near/far clipping planes then bring them in close to prevent clipping on scopes and such

	float nearZ = projection._43 / (projection._33 - 1.0f);

	float farZ = projection._43 / (projection._33 + 1.0f);

	float newNear = nearZ * 0.1f;
	float newFar = farZ * 0.1f;

	projection._33 = -(newFar + newNear) / (newFar - newNear);
	projection._43 = -(2.0f * newFar * newNear) / (newFar - newNear);
}

void InGameRenderer::Render(IDirect3DDevice9* pDevice)
{
	LPDIRECT3DSTATEBLOCK9 pStateBlock = NULL;
	if (FAILED(pDevice->CreateStateBlock(D3DSBT_ALL, &pStateBlock)) || !pStateBlock) return;

	pDevice->Clear(0, NULL, D3DCLEAR_STENCIL, D3DCOLOR_XRGB(0, 0, 0), 0.0f, 0);

	Draw2DLines(pDevice);

	Draw2DPolygons(pDevice);

	Draw3DLines(pDevice);

	DrawPolygons(pDevice);

	DrawRenderTargets(pDevice);
	DrawMeshes(pDevice);
    DrawLabels(pDevice);

	pStateBlock->Apply();
	pStateBlock->Release();
}

void InGameRenderer::PostRender()
{
	lines2D.clear();
	polygons2D.clear();
	lines3D.clear();
	depthLines3D.clear();
	meshVertices.clear();
    labels.clear();
	renderTargets.clear();
	polygons.clear();
	depthPolygons.clear();
}

void InGameRenderer::ClearRenderTargets()
{
	renderTargets.clear();
}

void InGameRenderer::DrawMeshTriangle(const Vector3 (&p)[3], const D3DCOLOR (&colors)[3])
{
	for (unsigned i=0; i<3; ++i) meshVertices.push_back({p[i].x,p[i].y,p[i].z,colors[i]});
}

void InGameRenderer::DrawMeshes(IDirect3DDevice9* device)
{
	if (meshVertices.empty()) return;
    // A native HUD/view-model pass may leave a compressed depth range. Using
    // its viewport with our first-person projection puts the surface at a false depth.
    D3DVIEWPORT9 viewport{};
    if (FAILED(device->GetViewport(&viewport))) return;
    viewport.MinZ=0.f;viewport.MaxZ=1.f;device->SetViewport(&viewport);
	// Explicit state: Halo can leave a weapon clip plane, scissor, vertex
    // blending and color masks active. Empty hands have no native weapon pass
    // to reset them. Render() restores the complete previous device state.
    device->SetRenderState(D3DRS_CLIPPLANEENABLE,0);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);
    device->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
    device->SetRenderState(D3DRS_VERTEXBLEND,D3DVBF_DISABLE);
    device->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE,FALSE);
    device->SetRenderState(D3DRS_COLORWRITEENABLE,15);
    device->SetRenderState(D3DRS_DEPTHBIAS,0);
    device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,0);
    device->SetVertexShader(nullptr);
	device->SetPixelShader(nullptr);
	device->SetTexture(0,nullptr);
	device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
	device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
	device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);
	device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
	device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
	device->SetRenderState(D3DRS_LIGHTING,FALSE);
	device->SetRenderState(D3DRS_FOGENABLE,FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
	device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
	device->SetRenderState(D3DRS_STENCILENABLE,FALSE);
	device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
	device->SetRenderState(D3DRS_ZENABLE,TRUE);
	device->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);
	device->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
	device->SetRenderState(D3DRS_SHADEMODE,D3DSHADE_GOURAUD);
	device->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE);
	device->SetTransform(D3DTS_WORLD,&world);
	device->SetTransform(D3DTS_VIEW,&view);
	device->SetTransform(D3DTS_PROJECTION,&meshProjection);
	device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,static_cast<UINT>(meshVertices.size()/3),meshVertices.data(),sizeof(VertexData3D));
}

void InGameRenderer::DrawLabel3D(const Font& font, const char* text, const Vector3& center,
    const Vector3& right, const Vector3& up, float height, float minimumWidth, bool respectDepth)
{
    if (!font.fontBitmap || font.lineHeight<=0 || !text || !*text || height<=0) return;
    float width=0;
    unsigned count=0;
    for (; count<64 && text[count]; ++count) {
        const unsigned ch=static_cast<unsigned char>(text[count]);
        width+=font.cdata[(ch>=32 && ch<=127 ? ch : '?')-32].xadvance;
    }
    const float unit=height/font.lineHeight;
    Label3D label; label.texture=font.fontBitmap;label.respectDepth=respectDepth;
    auto quad=[&](std::vector<LabelVertex>& vertices, float x0,float y0,float x1,float y1,
        DWORD color,float u0=0,float v0=0,float u1=0,float v1=0) {
        const Vector3 p[4]={center+right*x0+up*y0,center+right*x0+up*y1,
            center+right*x1+up*y0,center+right*x1+up*y1};
        const LabelVertex v[4]={{p[0].x,p[0].y,p[0].z,color,u0,v0},
            {p[1].x,p[1].y,p[1].z,color,u0,v1},{p[2].x,p[2].y,p[2].z,color,u1,v0},
            {p[3].x,p[3].y,p[3].z,color,u1,v1}};
        for (unsigned i:{0u,1u,2u,2u,1u,3u}) vertices.push_back(v[i]);
    };
    const float contentWidth=width*unit+height*0.8f;
    const float halfWidth=(contentWidth>minimumWidth ? contentWidth : minimumWidth)*0.5f;
    const float halfHeight=height*0.8f, edge=height*0.06f;
    quad(label.background,-halfWidth,halfHeight,halfWidth,-halfHeight,D3DCOLOR_ARGB(215,12,21,26));
    // A subdued frame remains visible around the EMPTY badge in dark rooms.
    const auto border=D3DCOLOR_ARGB(170,112,157,165);
    quad(label.background,-halfWidth,halfHeight,halfWidth,halfHeight-edge,border);
    quad(label.background,-halfWidth,-halfHeight+edge,halfWidth,-halfHeight,border);
    quad(label.background,-halfWidth,halfHeight-edge,-halfWidth+edge,-halfHeight+edge,border);
    quad(label.background,halfWidth-edge,halfHeight-edge,halfWidth,-halfHeight+edge,border);
    float x=-width*0.5f;
    for (unsigned i=0;i<count;++i) {
        const unsigned ch=static_cast<unsigned char>(text[i]);
        const auto& glyph=font.cdata[(ch>=32 && ch<=127 ? ch : '?')-32];
        const float y=font.offset+glyph.yoff;
        quad(label.glyphs,(x+glyph.xoff)*unit,(font.lineHeight*0.5f-y)*unit,
            (x+glyph.xoff+glyph.x1-glyph.x0)*unit,
            (font.lineHeight*0.5f-y-glyph.y1+glyph.y0)*unit,D3DCOLOR_ARGB(245,222,234,238),
            glyph.x0/512.0f,glyph.y0/512.0f,glyph.x1/512.0f,glyph.y1/512.0f);
        x+=glyph.xadvance;
    }
    labels.push_back(std::move(label));
}

void InGameRenderer::DrawLabels(IDirect3DDevice9* device)
{
    if (labels.empty()) return;
    // All state is restored by Render's enclosing state block, for each eye.
    device->SetVertexShader(nullptr); device->SetPixelShader(nullptr);
    device->SetRenderState(D3DRS_CLIPPLANEENABLE,0);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);
    device->SetRenderState(D3DRS_VERTEXBLEND,D3DVBF_DISABLE);
    device->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE,FALSE);
    device->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
    device->SetRenderState(D3DRS_COLORWRITEENABLE,15);
    device->SetRenderState(D3DRS_LIGHTING,FALSE); device->SetRenderState(D3DRS_FOGENABLE,FALSE);
    device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    device->SetRenderState(D3DRS_ZENABLE,FALSE); device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
    device->SetRenderState(D3DRS_STENCILENABLE,FALSE); device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
    device->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);
    device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE);
    device->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE);
    device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
    device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
    device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
    device->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
    device->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);
    device->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
    device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
    device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
    device->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
    device->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
    device->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
    device->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);
    device->SetTransform(D3DTS_WORLD,&world); device->SetTransform(D3DTS_VIEW,&view);
    device->SetTransform(D3DTS_PROJECTION,&projection);
    for (const auto& label:labels) {
        if(label.respectDepth){
            D3DVIEWPORT9 viewport{};
            if(FAILED(device->GetViewport(&viewport)))continue;
            viewport.MinZ=0.f;viewport.MaxZ=1.f;device->SetViewport(&viewport);
        }
        device->SetRenderState(D3DRS_ZENABLE,label.respectDepth?TRUE:FALSE);
        device->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
        device->SetTransform(D3DTS_PROJECTION,label.respectDepth?&meshProjection:&projection);
        device->SetTexture(0,nullptr);
        device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG2);
        device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,static_cast<UINT>(label.background.size()/3),
            label.background.data(),sizeof(LabelVertex));
        device->SetTexture(0,label.texture);
        device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);
        device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,static_cast<UINT>(label.glyphs.size()/3),
            label.glyphs.data(),sizeof(LabelVertex));
    }
}

//================================//Internal Functions//================================//

void InGameRenderer::Draw2DLines(IDirect3DDevice9* pDevice)
{
	pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
	pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pDevice->SetRenderState(D3DRS_LIGHTING, false);
	pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);

	pDevice->DrawPrimitiveUP(D3DPT_LINELIST, vertex2DCount / 2, vertices2D, sizeof(VertexData2D));

}

void InGameRenderer::Draw2DPolygons(IDirect3DDevice9* pDevice)
{
	pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
	pDevice->SetRenderState(D3DRS_LIGHTING, false);
	pDevice->SetRenderState(D3DRS_ZENABLE, false);
	pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);

	for (Polygon2D& polygon : polygons2D)
	{
		pDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, polygon.vertices.size() - 2, polygon.vertices.data(), sizeof(VertexData2D));
	}
}

void InGameRenderer::Draw3DLines(IDirect3DDevice9* pDevice)
{
	if (lines3D.empty() && depthLines3D.empty())
	{
		return;
	}

	pDevice->SetRenderState(D3DRS_ZENABLE, true);
	pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
	pDevice->SetTransform(D3DTS_WORLD, &world);
	pDevice->SetTransform(D3DTS_VIEW, &view);
	pDevice->SetTransform(D3DTS_PROJECTION, &projection);

	pDevice->DrawPrimitiveUP(D3DPT_LINELIST, depthLines3D.size() / 2, depthLines3D.data(), sizeof(VertexData3D));
	pDevice->SetRenderState(D3DRS_ZENABLE, false);
	pDevice->DrawPrimitiveUP(D3DPT_LINELIST, lines3D.size() / 2, lines3D.data(), sizeof(VertexData3D));
}

void InGameRenderer::DrawRenderTargets(IDirect3DDevice9* pDevice)
{
	// Normal blending, assumes texture actually has an alpha value
	//*
	pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	pDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE); // Intercepted UI layers are premultiplied
	pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	//*/

	// Greyscale alpha
	/*
	pDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
	pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);

	pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE | D3DTA_ALPHAREPLICATE);

	// Disable further stages
	pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	//*/

	pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);
	pDevice->SetTransform(D3DTS_WORLD, &world);
	pDevice->SetTransform(D3DTS_VIEW, &view);
	pDevice->SetTransform(D3DTS_PROJECTION, &projection);


	pDevice->SetRenderState(D3DRS_STENCILREF, 1);
	pDevice->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
	pDevice->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
	pDevice->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
	pDevice->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);

	static WORD indices[] = {
		0, 1, 3, 2, 3, 1
	};

	pDevice->SetRenderState(D3DRS_STENCILENABLE, 1);

	for (size_t i = 0; i < renderTargets.size(); i++)
	{
		Vector3 centre = renderTargets[i].pos;
		Vector3 sizeTR = renderTargets[i].rot * Vector3(renderTargets[i].size.x, 0.0f, renderTargets[i].size.y);
		Vector3 sizeBR = renderTargets[i].rot * Vector3(renderTargets[i].size.x, 0.0f, -renderTargets[i].size.y);
		Vector3 sizeBL = renderTargets[i].rot * Vector3(-renderTargets[i].size.x, 0.0f, -renderTargets[i].size.y);
		Vector3 sizeTL = renderTargets[i].rot * Vector3(-renderTargets[i].size.x, 0.0f, renderTargets[i].size.y);

		VertexDataTex vertices[] = {
			{centre.x + sizeTR.x, centre.y + sizeTR.y, centre.z + sizeTR.z, 1.0f, 0.0f},
			{centre.x + sizeBR.x, centre.y + sizeBR.y, centre.z + sizeBR.z, 1.0f, 1.0f},
			{centre.x + sizeBL.x, centre.y + sizeBL.y, centre.z + sizeBL.z, 0.0f, 1.0f},
			{centre.x + sizeTL.x, centre.y + sizeTL.y, centre.z + sizeTL.z, 0.0f, 0.0f}
		};

		pDevice->SetRenderState(D3DRS_ZENABLE, renderTargets[i].bRespectDepth);

		pDevice->SetRenderState(D3DRS_STENCILFUNC, renderTargets[i].bRespectStencil ? D3DCMP_EQUAL : D3DCMP_NOTEQUAL);

		pDevice->SetTexture(0, renderTargets[i].texture);

		pDevice->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 4, 2, indices, D3DFMT_INDEX16, vertices, sizeof(VertexDataTex));
	}
}

void InGameRenderer::DrawPolygons(IDirect3DDevice9* pDevice)
{
	if (polygons.empty() && depthPolygons.empty())
	{
		return;
	}

	pDevice->SetRenderState(D3DRS_ZENABLE, true);
	pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
	pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
	pDevice->SetTransform(D3DTS_WORLD, &world);
	pDevice->SetTransform(D3DTS_VIEW, &view);
	pDevice->SetTransform(D3DTS_PROJECTION, &projection);

	for (Polygon polygon : depthPolygons)
	{
		pDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, polygon.vertices.size() - 2, polygon.vertices.data(), sizeof(VertexData3D));
	}
	pDevice->SetRenderState(D3DRS_ZENABLE, false);

	pDevice->SetRenderState(D3DRS_STENCILREF, 1);
	pDevice->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
	pDevice->SetRenderState(D3DRS_STENCILMASK, 0xFFFFFFFF);
	pDevice->SetRenderState(D3DRS_STENCILWRITEMASK, 0XFFFFFFFF);
	pDevice->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
	pDevice->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
	pDevice->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);

	for (Polygon polygon : polygons)
	{

		pDevice->SetRenderState(D3DRS_STENCILENABLE, polygon.bIsStencil);

		pDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, polygon.vertices.size() - 2, polygon.vertices.data(), sizeof(VertexData3D));
	}
}
