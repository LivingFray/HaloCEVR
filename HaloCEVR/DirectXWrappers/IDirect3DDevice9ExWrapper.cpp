#include "IDirect3DDevice9ExWrapper.h"
#include "../Logger.h"
#include "../Game.h"
#include "../Immersion/GreyMarkerDraw.h"

IDirect3DPixelShader9* IDirect3DDevice9ExWrapper::MarkerShader()
{
    if (!bGreyHolsterMarker) return nullptr;
    if (!markerShaderAttempted) {
        markerShaderAttempted=true;
        // ps_2_0; def c0, .2, .2, .2, 1; mov oC0, c0; end.
        // A dark opaque silhouette needs no texture/vertex semantics and no
        // runtime HLSL compiler DLL. In particular it does not alter map shaders.
        const DWORD code[]={0xffff0200,0x05000051,0xa00f0000,
            0x3e4ccccd,0x3e4ccccd,0x3e4ccccd,0x3f800000,
            0x02000001,0x800f0800,0xa0e40000,0x0000ffff};
        const auto result=Real->CreatePixelShader(code,&markerShader);
        Logger::log << "[PhysicalVisuals] grey marker shader result=" << result << std::endl;
    }
    return markerShader;
}
void IDirect3DDevice9ExWrapper::ReleaseMarkerShader()
{
    if (markerShader) { markerShader->Release(); markerShader=nullptr; }
    markerShaderAttempted=false;
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::QueryInterface(REFIID riid, void** ppvObj)
{
	return Real->QueryInterface(riid, ppvObj);
}

ULONG __stdcall IDirect3DDevice9ExWrapper::AddRef(void)
{
	return Real->AddRef();
}

ULONG __stdcall IDirect3DDevice9ExWrapper::Release(void)
{
    // Avoid keeping the device alive through our own shader reference.
    ReleaseMarkerShader();
	return Real->Release();
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::TestCooperativeLevel(void)
{
	return Real->TestCooperativeLevel();
}

UINT __stdcall IDirect3DDevice9ExWrapper::GetAvailableTextureMem(void)
{
	return Real->GetAvailableTextureMem();
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::EvictManagedResources(void)
{
	return Real->EvictManagedResources();
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetDirect3D(IDirect3D9** ppD3D9)
{
	return Real->GetDirect3D(ppD3D9);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetDeviceCaps(D3DCAPS9* pCaps)
{
	return Real->GetDeviceCaps(pCaps);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode)
{
	return Real->GetDisplayMode(iSwapChain, pMode);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* pParameters)
{
	return Real->GetCreationParameters(pParameters);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap)
{
    return Real->SetCursorProperties(XHotSpot, YHotSpot, pCursorBitmap);
}

void __stdcall IDirect3DDevice9ExWrapper::SetCursorPosition(int X, int Y, DWORD Flags)
{
	Real->SetCursorPosition(X, Y, Flags);
}

BOOL __stdcall IDirect3DDevice9ExWrapper::ShowCursor(BOOL bShow)
{
	return Real->ShowCursor(bShow);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** pSwapChain)
{
	return Real->CreateAdditionalSwapChain(pPresentationParameters, pSwapChain);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain)
{
	return Real->GetSwapChain(iSwapChain, pSwapChain);
}

UINT __stdcall IDirect3DDevice9ExWrapper::GetNumberOfSwapChains(void)
{
	return Real->GetNumberOfSwapChains();
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters)
{
    ReleaseMarkerShader();
	if (pPresentationParameters->BackBufferWidth != 1200 || pPresentationParameters->BackBufferHeight != 600)
	{
		pPresentationParameters->BackBufferWidth = Game::instance.backBufferWidth;
		pPresentationParameters->BackBufferHeight = Game::instance.backBufferHeight;
	}
	return Real->Reset(pPresentationParameters);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::Present(const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion)
{
	return Real->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9** ppBackBuffer)
{
	return Real->GetBackBuffer(iSwapChain, iBackBuffer, Type, ppBackBuffer);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus)
{
	return Real->GetRasterStatus(iSwapChain, pRasterStatus);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetDialogBoxMode(BOOL bEnableDialogs)
{
	return Real->SetDialogBoxMode(bEnableDialogs);
}

void __stdcall IDirect3DDevice9ExWrapper::SetGammaRamp(UINT iSwapChain, DWORD Flags, const D3DGAMMARAMP* pRamp)
{
	Real->SetGammaRamp(iSwapChain, Flags, pRamp);
}

void __stdcall IDirect3DDevice9ExWrapper::GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp)
{
	Real->GetGammaRamp(iSwapChain, pRamp);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle)
{
	if (Pool == D3DPOOL_MANAGED) { Pool = D3DPOOL_DEFAULT; Usage |= D3DUSAGE_DYNAMIC; }; // 9Ex dropped support for managed pools, apparently default will work fine
	return Real->CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle)
{
	if (Pool == D3DPOOL_MANAGED) { Pool = D3DPOOL_DEFAULT; Usage |= D3DUSAGE_DYNAMIC; }; // 9Ex dropped support for managed pools, apparently default will work fine
	return Real->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture, pSharedHandle);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle)
{
	if (Pool == D3DPOOL_MANAGED) { Pool = D3DPOOL_DEFAULT; Usage |= D3DUSAGE_DYNAMIC; }; // 9Ex dropped support for managed pools, apparently default will work fine
	return Real->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture, pSharedHandle);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, HANDLE* pSharedHandle)
{
	if (Pool == D3DPOOL_MANAGED) { Pool = D3DPOOL_DEFAULT; Usage |= D3DUSAGE_DYNAMIC; }; // 9Ex dropped support for managed pools, apparently default will work fine
	return Real->CreateVertexBuffer(Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle)
{
	if (Pool == D3DPOOL_MANAGED) { Pool = D3DPOOL_DEFAULT; Usage |= D3DUSAGE_DYNAMIC; }; // 9Ex dropped support for managed pools, apparently default will work fine
	return Real->CreateIndexBuffer(Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle)
{
	return Real->CreateRenderTarget(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle)
{
	return Real->CreateDepthStencilSurface(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::UpdateSurface(IDirect3DSurface9* pSourceSurface, const RECT* pSourceRect, IDirect3DSurface9* pDestinationSurface, const POINT* pDestPoint)
{
	return Real->UpdateSurface(pSourceSurface, pSourceRect, pDestinationSurface, pDestPoint);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::UpdateTexture(IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture)
{
	return Real->UpdateTexture(pSourceTexture, pDestinationTexture);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetRenderTargetData(IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface)
{
	return Real->GetRenderTargetData(pRenderTarget, pDestSurface);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface)
{
	return Real->GetFrontBufferData(iSwapChain, pDestSurface);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::StretchRect(IDirect3DSurface9* pSourceSurface, const RECT* pSourceRect, IDirect3DSurface9* pDestSurface, const RECT* pDestRect, D3DTEXTUREFILTERTYPE Filter)
{
	return Real->StretchRect(pSourceSurface, pSourceRect, pDestSurface, pDestRect, Filter);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::ColorFill(IDirect3DSurface9* pSurface, const RECT* pRect, D3DCOLOR color)
{
	return Real->ColorFill(pSurface, pRect, color);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle)
{
	if (Pool == D3DPOOL_MANAGED) Pool = D3DPOOL_DEFAULT; // 9Ex dropped support for managed pools, apparently default will work fine
	return Real->CreateOffscreenPlainSurface(Width, Height, Format, Pool, ppSurface, pSharedHandle);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget)
{
	/*
	RenderTarget* renderTargets = Helpers::GetRenderTargets();

	for (int i = 0; i < 8; i++)
	{
		if (renderTargets[i].renderSurface == pRenderTarget)
		{
			D3DSURFACE_DESC desc;
			pRenderTarget->GetDesc(&desc);

			Logger::log << "SetRenderTarget (" << i << ") " << desc.Width << "x" << desc.Height << " 0x" << std::hex << pRenderTarget << std::dec << std::endl;
			Logger::log << "Stored Size: " << renderTargets[i].width << "x" << renderTargets[i].height << std::endl;
			break;
		}
	}
	//*/

	return Real->SetRenderTarget(RenderTargetIndex, pRenderTarget);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget)
{
	return Real->GetRenderTarget(RenderTargetIndex, ppRenderTarget);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil)
{
	return Real->SetDepthStencilSurface(pNewZStencil);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface)
{
	return Real->GetDepthStencilSurface(ppZStencilSurface);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::BeginScene(void)
{
	if (bIgnoreNextStart)
	{
		bIgnoreNextStart = false;
		return S_OK;
	}
	return Real->BeginScene();
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::EndScene(void)
{
	if (bIgnoreNextEnd)
	{
		bIgnoreNextEnd = false;
		return S_OK;
	}
	return Real->EndScene();
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::Clear(DWORD Count, const D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil)
{
    return Real->Clear(Count, pRects, Flags, Color, Z, Stencil);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix)
{
    return Real->SetTransform(State, pMatrix);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix)
{
    return Real->GetTransform(State, pMatrix);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::MultiplyTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix)
{
    return Real->MultiplyTransform(State, pMatrix);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetViewport(const D3DVIEWPORT9* pViewport)
{
	//Logger::log << "SetViewport " << pViewport->Width << "x" << pViewport->Height << std::endl;
    return Real->SetViewport(pViewport);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetViewport(D3DVIEWPORT9* pViewport)
{
    return Real->GetViewport(pViewport);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetMaterial(const D3DMATERIAL9* pMaterial)
{
    return Real->SetMaterial(pMaterial);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetMaterial(D3DMATERIAL9* pMaterial)
{
    return Real->GetMaterial(pMaterial);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetLight(DWORD Index, const D3DLIGHT9* Light)
{
    return Real->SetLight(Index, Light);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetLight(DWORD Index, D3DLIGHT9* Light)
{
    return Real->GetLight(Index, Light);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::LightEnable(DWORD Index, BOOL Enable)
{
	return Real->LightEnable(Index, Enable);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetLightEnable(DWORD Index, BOOL* pEnable)
{
	return Real->GetLightEnable(Index, pEnable);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetClipPlane(DWORD Index, const float* pPlane)
{
	return Real->SetClipPlane(Index, pPlane);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetClipPlane(DWORD Index, float* pPlane)
{
	return Real->GetClipPlane(Index, pPlane);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value)
{
	if (State == D3DRS_CULLMODE && bSkipWinding)
	{
		return S_OK;
	}

	return Real->SetRenderState(State, Value);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue)
{
	return Real->GetRenderState(State, pValue);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB)
{
	return Real->CreateStateBlock(Type, ppSB);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::BeginStateBlock(void)
{
	return Real->BeginStateBlock();
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::EndStateBlock(IDirect3DStateBlock9** ppSB)
{
	return Real->EndStateBlock(ppSB);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetClipStatus(const D3DCLIPSTATUS9* pClipStatus)
{
	return Real->SetClipStatus(pClipStatus);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetClipStatus(D3DCLIPSTATUS9* pClipStatus)
{
	return Real->GetClipStatus(pClipStatus);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetTexture(DWORD Stage, IDirect3DBaseTexture9** ppTexture)
{
	return Real->GetTexture(Stage, ppTexture);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetTexture(DWORD Stage, IDirect3DBaseTexture9* pTexture)
{
	return Real->SetTexture(Stage, pTexture);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue)
{
	return Real->GetTextureStageState(Stage, Type, pValue);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
	return Real->SetTextureStageState(Stage, Type, Value);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue)
{
	return Real->GetSamplerState(Sampler, Type, pValue);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
{
	return Real->SetSamplerState(Sampler, Type, Value);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::ValidateDevice(DWORD* pNumPasses)
{
	return Real->ValidateDevice(pNumPasses);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetPaletteEntries(UINT PaletteNumber, const PALETTEENTRY* pEntries)
{
	return Real->SetPaletteEntries(PaletteNumber, pEntries);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries)
{
	return Real->GetPaletteEntries(PaletteNumber, pEntries);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetCurrentTexturePalette(UINT PaletteNumber)
{
	return Real->SetCurrentTexturePalette(PaletteNumber);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetCurrentTexturePalette(UINT* PaletteNumber)
{
	return Real->GetCurrentTexturePalette(PaletteNumber);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetScissorRect(const RECT* pRect)
{
	return Real->SetScissorRect(pRect);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetScissorRect(RECT* pRect)
{
	return Real->GetScissorRect(pRect);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetSoftwareVertexProcessing(BOOL bSoftware)
{
	return Real->SetSoftwareVertexProcessing(bSoftware);
}

BOOL __stdcall IDirect3DDevice9ExWrapper::GetSoftwareVertexProcessing(void)
{
	return Real->GetSoftwareVertexProcessing();
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetNPatchMode(float nSegments)
{
	return Real->SetNPatchMode(nSegments);
}

float __stdcall IDirect3DDevice9ExWrapper::GetNPatchMode(void)
{
	return Real->GetNPatchMode();
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount)
{
    GreyMarkerDraw<IDirect3DDevice9Ex,IDirect3DPixelShader9> marker(Real,MarkerShader());
	return Real->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount)
{
    GreyMarkerDraw<IDirect3DDevice9Ex,IDirect3DPixelShader9> marker(Real,MarkerShader());
	return Real->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
    GreyMarkerDraw<IDirect3DDevice9Ex,IDirect3DPixelShader9> marker(Real,MarkerShader());
	return Real->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, const void* pIndexData, D3DFORMAT IndexDataFormat, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
    GreyMarkerDraw<IDirect3DDevice9Ex,IDirect3DPixelShader9> marker(Real,MarkerShader());
    return Real->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer, IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags)
{
    return Real->ProcessVertices(SrcStartIndex, DestIndex, VertexCount, pDestBuffer, pVertexDecl, Flags);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::CreateVertexDeclaration(const D3DVERTEXELEMENT9* pVertexElements, IDirect3DVertexDeclaration9** ppDecl)
{
    return Real->CreateVertexDeclaration(pVertexElements, ppDecl);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl)
{
    return Real->SetVertexDeclaration(pDecl);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl)
{
    return Real->GetVertexDeclaration(ppDecl);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetFVF(DWORD FVF)
{
    return Real->SetFVF(FVF);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetFVF(DWORD* pFVF)
{
    return Real->GetFVF(pFVF);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::CreateVertexShader(const DWORD* pFunction, IDirect3DVertexShader9** ppShader)
{
    return Real->CreateVertexShader(pFunction, ppShader);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetVertexShader(IDirect3DVertexShader9* pShader)
{
	return Real->SetVertexShader(pShader);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetVertexShader(IDirect3DVertexShader9** ppShader)
{
	return Real->GetVertexShader(ppShader);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetVertexShaderConstantF(UINT StartRegister, const float* pConstantData, UINT Vector4fCount)
{
	return Real->SetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetVertexShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount)
{
	return Real->GetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetVertexShaderConstantI(UINT StartRegister, const int* pConstantData, UINT Vector4iCount)
{
	return Real->SetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetVertexShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount)
{
	return Real->GetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetVertexShaderConstantB(UINT StartRegister, const BOOL* pConstantData, UINT BoolCount)
{
	return Real->SetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetVertexShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount)
{
	return Real->GetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride)
{
	return Real->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9** ppStreamData, UINT* pOffsetInBytes, UINT* pStride)
{
	return Real->GetStreamSource(StreamNumber, ppStreamData, pOffsetInBytes, pStride);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetStreamSourceFreq(UINT StreamNumber, UINT Setting)
{
	return Real->SetStreamSourceFreq(StreamNumber, Setting);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetStreamSourceFreq(UINT StreamNumber, UINT* pSetting)
{
	return Real->GetStreamSourceFreq(StreamNumber, pSetting);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetIndices(IDirect3DIndexBuffer9* pIndexData)
{
	return Real->SetIndices(pIndexData);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetIndices(IDirect3DIndexBuffer9** ppIndexData)
{
	return Real->GetIndices(ppIndexData);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::CreatePixelShader(const DWORD* pFunction, IDirect3DPixelShader9** ppShader)
{
	return Real->CreatePixelShader(pFunction, ppShader);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetPixelShader(IDirect3DPixelShader9* pShader)
{
	return Real->SetPixelShader(pShader);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetPixelShader(IDirect3DPixelShader9** ppShader)
{
	return Real->GetPixelShader(ppShader);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetPixelShaderConstantF(UINT StartRegister, const float* pConstantData, UINT Vector4fCount)
{
	return Real->SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetPixelShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount)
{
	return Real->GetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetPixelShaderConstantI(UINT StartRegister, const int* pConstantData, UINT Vector4iCount)
{
	return Real->SetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetPixelShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount)
{
	return Real->GetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetPixelShaderConstantB(UINT StartRegister, const BOOL* pConstantData, UINT BoolCount)
{
	return Real->SetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetPixelShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount)
{
	return Real->GetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::DrawRectPatch(UINT Handle, const float* pNumSegs, const D3DRECTPATCH_INFO* pRectPatchInfo)
{
    return Real->DrawRectPatch(Handle, pNumSegs, pRectPatchInfo);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::DrawTriPatch(UINT Handle, const float* pNumSegs, const D3DTRIPATCH_INFO* pTriPatchInfo)
{
    return Real->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::DeletePatch(UINT Handle)
{
    return Real->DeletePatch(Handle);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery)
{
    return Real->CreateQuery(Type, ppQuery);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetConvolutionMonoKernel(UINT width, UINT height, float* rows, float* columns)
{
    return Real->SetConvolutionMonoKernel(width, height, rows, columns);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::ComposeRects(IDirect3DSurface9* pSrc, IDirect3DSurface9* pDst, IDirect3DVertexBuffer9* pSrcRectDescs, UINT NumRects, IDirect3DVertexBuffer9* pDstRectDescs, D3DCOMPOSERECTSOP Operation, int Xoffset, int Yoffset)
{
    return Real->ComposeRects(pSrc, pDst, pSrcRectDescs, NumRects, pDstRectDescs, Operation, Xoffset, Yoffset);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::PresentEx(const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion, DWORD dwFlags)
{
    return Real->PresentEx(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetGPUThreadPriority(INT* pPriority)
{
    return Real->GetGPUThreadPriority(pPriority);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetGPUThreadPriority(INT Priority)
{
	return Real->SetGPUThreadPriority(Priority);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::WaitForVBlank(UINT iSwapChain)
{
	return Real->WaitForVBlank(iSwapChain);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::CheckResourceResidency(IDirect3DResource9** pResourceArray, UINT32 NumResources)
{
	return Real->CheckResourceResidency(pResourceArray, NumResources);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::SetMaximumFrameLatency(UINT MaxLatency)
{
	return Real->SetMaximumFrameLatency(MaxLatency);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetMaximumFrameLatency(UINT* pMaxLatency)
{
	return Real->GetMaximumFrameLatency(pMaxLatency);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::CheckDeviceState(HWND hDestinationWindow)
{
	return Real->CheckDeviceState(hDestinationWindow);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::CreateRenderTargetEx(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle, DWORD Usage)
{
	return Real->CreateRenderTargetEx(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle, Usage);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::CreateOffscreenPlainSurfaceEx(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle, DWORD Usage)
{
	if (Pool == D3DPOOL_MANAGED) { Pool = D3DPOOL_DEFAULT; Usage |= D3DUSAGE_DYNAMIC; }; // 9Ex dropped support for managed pools, apparently default will work fine
	return Real->CreateOffscreenPlainSurfaceEx(Width, Height, Format, Pool, ppSurface, pSharedHandle, Usage);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::CreateDepthStencilSurfaceEx(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle, DWORD Usage)
{
	return Real->CreateDepthStencilSurfaceEx(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle, Usage);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::ResetEx(D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode)
{
    ReleaseMarkerShader();
	return Real->ResetEx(pPresentationParameters, pFullscreenDisplayMode);
}

HRESULT __stdcall IDirect3DDevice9ExWrapper::GetDisplayModeEx(UINT iSwapChain, D3DDISPLAYMODEEX* pMode, D3DDISPLAYROTATION* pRotation)
{
	return Real->GetDisplayModeEx(iSwapChain, pMode, pRotation);
}
