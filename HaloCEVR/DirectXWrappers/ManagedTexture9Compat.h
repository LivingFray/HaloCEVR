#pragma once

#include <d3d9.h>
#include <atomic>
#include <cstring>
#include <mutex>
#include <new>
#include <unordered_map>

#include "../Logger.h"

// IDirect3DDevice9Ex rejects D3DPOOL_MANAGED.  HaloCEVR historically changed
// managed textures to DEFAULT | DYNAMIC, but that is not equivalent: managed
// resources are lockable system-memory shadows which the D3D9 runtime uploads
// to device memory when required.  Some Refined/Restored bitmap formats depend
// on those exact upload semantics.
//
// This header implements the missing behaviour without changing the Ex device
// OpenVR needs for shared eye textures.  Halo receives a COM-compatible texture
// wrapper backed by a SYSTEMMEM texture and a matching DEFAULT texture.  Locks
// target the shadow; draw calls bind the uploaded GPU copy.
namespace D3D9ManagedCompat
{
	inline bool SameGuid(REFIID left, const GUID& right)
	{
		return std::memcmp(&left, &right, sizeof(GUID)) == 0;
	}

	// Local GUID values avoid a new dxguid.lib dependency.
	constexpr GUID IID_UnknownLocal =
		{ 0x00000000, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
	constexpr GUID IID_Resource9Local =
		{ 0x05eec05d, 0x8f7d, 0x4362, { 0xb9, 0x99, 0xd1, 0xba, 0xf3, 0x57, 0xc7, 0x04 } };
	constexpr GUID IID_BaseTexture9Local =
		{ 0x580ca87e, 0x1d3c, 0x4d54, { 0x99, 0x1d, 0xb7, 0xd3, 0xe3, 0xc2, 0x98, 0xce } };
	constexpr GUID IID_Texture9Local =
		{ 0x85c31227, 0x3de5, 0x4f00, { 0x9b, 0x3a, 0xf1, 0x1a, 0xc3, 0x8c, 0x18, 0xb5 } };
	constexpr GUID IID_CubeTexture9Local =
		{ 0xfff32f81, 0xd953, 0x473a, { 0x92, 0x23, 0x93, 0xd6, 0x52, 0xab, 0xa9, 0x3f } };
	constexpr GUID IID_VolumeTexture9Local =
		{ 0x2518526c, 0xe789, 0x4111, { 0xa7, 0xb9, 0x47, 0xef, 0x32, 0x8d, 0x13, 0xe6 } };
	constexpr GUID IID_Surface9Local =
		{ 0x0cfbaf3a, 0x9ff6, 0x429a, { 0x99, 0xb3, 0xa2, 0x79, 0x6a, 0xf8, 0xb8, 0x9b } };
	constexpr GUID IID_Volume9Local =
		{ 0x24f416e6, 0x1f67, 0x4aa7, { 0xb8, 0x8e, 0xd3, 0x3f, 0x6f, 0x31, 0x28, 0xa1 } };

	class ManagedTextureControl
	{
	public:
		virtual ~ManagedTextureControl() = default;
		virtual IDirect3DBaseTexture9* ApplicationTexture() = 0;
		virtual IDirect3DBaseTexture9* ShadowTexture() = 0;
		virtual IDirect3DBaseTexture9* GPUTexture() = 0;
		virtual ULONG AddRefApplication() = 0;
		virtual ULONG ReleaseApplication() = 0;
		virtual HRESULT QueryApplicationInterface(REFIID riid, void** object) = 0;
		virtual HRESULT ReturnOwnerDevice(IDirect3DDevice9** device) = 0;
		virtual HRESULT SyncAll(const char* reason) = 0;
		virtual HRESULT SyncSurface(bool cube, D3DCUBEMAP_FACES face, UINT level) = 0;
		virtual HRESULT SyncVolume(UINT level) = 0;
		virtual DWORD OriginalUsage() const = 0;
	};

	struct TextureRegistry
	{
		std::mutex lock;
		std::unordered_map<IDirect3DBaseTexture9*, ManagedTextureControl*> application;
		std::unordered_map<IDirect3DBaseTexture9*, ManagedTextureControl*> gpu;
	};

	inline TextureRegistry& Registry()
	{
		// Keep the registry alive until process teardown. D3D resources can be
		// released very late during DLL shutdown, after ordinary static destructors.
		static TextureRegistry* registry = new TextureRegistry();
		return *registry;
	}

	inline void RegisterTexture(ManagedTextureControl* texture)
	{
		TextureRegistry& registry = Registry();
		std::lock_guard<std::mutex> guard(registry.lock);
		registry.application[texture->ApplicationTexture()] = texture;
		registry.gpu[texture->GPUTexture()] = texture;
	}

	inline void UnregisterTexture(ManagedTextureControl* texture)
	{
		TextureRegistry& registry = Registry();
		std::lock_guard<std::mutex> guard(registry.lock);
		registry.application.erase(texture->ApplicationTexture());
		registry.gpu.erase(texture->GPUTexture());
	}

	inline ManagedTextureControl* FindApplicationTexture(IDirect3DBaseTexture9* texture)
	{
		if (!texture)
		{
			return nullptr;
		}

		TextureRegistry& registry = Registry();
		std::lock_guard<std::mutex> guard(registry.lock);
		auto found = registry.application.find(texture);
		return found == registry.application.end() ? nullptr : found->second;
	}

	inline ManagedTextureControl* FindGPUTexture(IDirect3DBaseTexture9* texture)
	{
		if (!texture)
		{
			return nullptr;
		}

		TextureRegistry& registry = Registry();
		std::lock_guard<std::mutex> guard(registry.lock);
		auto found = registry.gpu.find(texture);
		return found == registry.gpu.end() ? nullptr : found->second;
	}

	inline void LogFailure(const char* operation, HRESULT result, UINT level = 0)
	{
		Logger::log << "[D3DManagedCompat] " << operation
			<< " failed level=" << level
			<< " result=0x" << std::hex << static_cast<unsigned long>(result)
			<< std::dec << std::endl;
	}

	class ManagedSurfaceLevel final : public IDirect3DSurface9
	{
	public:
		ManagedSurfaceLevel(ManagedTextureControl* parent, IDirect3DSurface9* shadow,
			IDirect3DSurface9* gpu, bool cube, D3DCUBEMAP_FACES face, UINT level)
			: Parent(parent), Shadow(shadow), GPU(gpu), IsCube(cube), Face(face), Level(level)
		{
			Parent->AddRefApplication();
		}

		HRESULT __stdcall QueryInterface(REFIID riid, void** object) override
		{
			if (!object)
			{
				return E_POINTER;
			}
			*object = nullptr;
			if (SameGuid(riid, IID_UnknownLocal) || SameGuid(riid, IID_Resource9Local) || SameGuid(riid, IID_Surface9Local))
			{
				*object = static_cast<IDirect3DSurface9*>(this);
				AddRef();
				return S_OK;
			}
			return E_NOINTERFACE;
		}

		ULONG __stdcall AddRef() override { return ++References; }

		ULONG __stdcall Release() override
		{
			const ULONG remaining = --References;
			if (remaining == 0)
			{
				delete this;
			}
			return remaining;
		}

		HRESULT __stdcall GetDevice(IDirect3DDevice9** device) override { return Parent->ReturnOwnerDevice(device); }
		HRESULT __stdcall SetPrivateData(REFGUID guid, const void* data, DWORD size, DWORD flags) override { return Shadow->SetPrivateData(guid, data, size, flags); }
		HRESULT __stdcall GetPrivateData(REFGUID guid, void* data, DWORD* size) override { return Shadow->GetPrivateData(guid, data, size); }
		HRESULT __stdcall FreePrivateData(REFGUID guid) override { return Shadow->FreePrivateData(guid); }
		DWORD __stdcall SetPriority(DWORD priority) override { return GPU->SetPriority(priority); }
		DWORD __stdcall GetPriority() override { return GPU->GetPriority(); }

		void __stdcall PreLoad() override
		{
			const HRESULT result = Parent->SyncAll("surface PreLoad");
			if (FAILED(result)) LogFailure("surface PreLoad sync", result, Level);
			GPU->PreLoad();
		}

		D3DRESOURCETYPE __stdcall GetType() override { return D3DRTYPE_SURFACE; }
		HRESULT __stdcall GetContainer(REFIID riid, void** container) override { return Parent->QueryApplicationInterface(riid, container); }

		HRESULT __stdcall GetDesc(D3DSURFACE_DESC* desc) override
		{
			const HRESULT result = Shadow->GetDesc(desc);
			if (SUCCEEDED(result) && desc)
			{
				desc->Usage = Parent->OriginalUsage();
				desc->Pool = D3DPOOL_MANAGED;
			}
			return result;
		}

		HRESULT __stdcall LockRect(D3DLOCKED_RECT* locked, const RECT* rect, DWORD flags) override
		{
			const HRESULT result = Shadow->LockRect(locked, rect, flags);
			if (FAILED(result)) LogFailure("surface LockRect", result, Level);
			return result;
		}

		HRESULT __stdcall UnlockRect() override
		{
			const HRESULT result = Shadow->UnlockRect();
			if (FAILED(result))
			{
				LogFailure("surface UnlockRect", result, Level);
				return result;
			}

			const HRESULT syncResult = Parent->SyncSurface(IsCube, Face, Level);
			if (FAILED(syncResult)) LogFailure("surface upload", syncResult, Level);
			return result;
		}

		HRESULT __stdcall GetDC(HDC* dc) override { return Shadow->GetDC(dc); }

		HRESULT __stdcall ReleaseDC(HDC dc) override
		{
			const HRESULT result = Shadow->ReleaseDC(dc);
			if (SUCCEEDED(result))
			{
				const HRESULT syncResult = Parent->SyncSurface(IsCube, Face, Level);
				if (FAILED(syncResult)) LogFailure("surface DC upload", syncResult, Level);
			}
			return result;
		}

	private:
		~ManagedSurfaceLevel()
		{
			Shadow->Release();
			GPU->Release();
			Parent->ReleaseApplication();
		}

		std::atomic<ULONG> References{ 1 };
		ManagedTextureControl* Parent;
		IDirect3DSurface9* Shadow;
		IDirect3DSurface9* GPU;
		bool IsCube;
		D3DCUBEMAP_FACES Face;
		UINT Level;
	};

	class ManagedVolumeLevel final : public IDirect3DVolume9
	{
	public:
		ManagedVolumeLevel(ManagedTextureControl* parent, IDirect3DVolume9* shadow,
			IDirect3DVolume9* gpu, UINT level)
			: Parent(parent), Shadow(shadow), GPU(gpu), Level(level)
		{
			Parent->AddRefApplication();
		}

		HRESULT __stdcall QueryInterface(REFIID riid, void** object) override
		{
			if (!object) return E_POINTER;
			*object = nullptr;
			if (SameGuid(riid, IID_UnknownLocal) || SameGuid(riid, IID_Volume9Local))
			{
				*object = static_cast<IDirect3DVolume9*>(this);
				AddRef();
				return S_OK;
			}
			return E_NOINTERFACE;
		}

		ULONG __stdcall AddRef() override { return ++References; }

		ULONG __stdcall Release() override
		{
			const ULONG remaining = --References;
			if (remaining == 0) delete this;
			return remaining;
		}

		HRESULT __stdcall GetDevice(IDirect3DDevice9** device) override { return Parent->ReturnOwnerDevice(device); }
		HRESULT __stdcall SetPrivateData(REFGUID guid, const void* data, DWORD size, DWORD flags) override { return Shadow->SetPrivateData(guid, data, size, flags); }
		HRESULT __stdcall GetPrivateData(REFGUID guid, void* data, DWORD* size) override { return Shadow->GetPrivateData(guid, data, size); }
		HRESULT __stdcall FreePrivateData(REFGUID guid) override { return Shadow->FreePrivateData(guid); }
		HRESULT __stdcall GetContainer(REFIID riid, void** container) override { return Parent->QueryApplicationInterface(riid, container); }

		HRESULT __stdcall GetDesc(D3DVOLUME_DESC* desc) override
		{
			const HRESULT result = Shadow->GetDesc(desc);
			if (SUCCEEDED(result) && desc)
			{
				desc->Usage = Parent->OriginalUsage();
				desc->Pool = D3DPOOL_MANAGED;
			}
			return result;
		}

		HRESULT __stdcall LockBox(D3DLOCKED_BOX* locked, const D3DBOX* box, DWORD flags) override
		{
			const HRESULT result = Shadow->LockBox(locked, box, flags);
			if (FAILED(result)) LogFailure("volume level LockBox", result, Level);
			return result;
		}

		HRESULT __stdcall UnlockBox() override
		{
			const HRESULT result = Shadow->UnlockBox();
			if (FAILED(result))
			{
				LogFailure("volume level UnlockBox", result, Level);
				return result;
			}
			const HRESULT syncResult = Parent->SyncVolume(Level);
			if (FAILED(syncResult)) LogFailure("volume level upload", syncResult, Level);
			return result;
		}

	private:
		~ManagedVolumeLevel()
		{
			Shadow->Release();
			GPU->Release();
			Parent->ReleaseApplication();
		}

		std::atomic<ULONG> References{ 1 };
		ManagedTextureControl* Parent;
		IDirect3DVolume9* Shadow;
		IDirect3DVolume9* GPU;
		UINT Level;
	};

	template <typename Derived, typename Interface>
	class ManagedTextureCommon : public Interface, public ManagedTextureControl
	{
	public:
		ManagedTextureCommon(IDirect3DDevice9* owner, IDirect3DDevice9* realDevice,
			Interface* shadow, Interface* gpu, DWORD usage)
			: Owner(owner), RealDevice(realDevice), Shadow(shadow), GPU(gpu), Usage(usage)
		{
		}

		HRESULT __stdcall QueryInterface(REFIID riid, void** object) override
		{
			if (!object) return E_POINTER;
			*object = nullptr;
			if (SameGuid(riid, IID_UnknownLocal) || SameGuid(riid, IID_Resource9Local) ||
				SameGuid(riid, IID_BaseTexture9Local) || static_cast<Derived*>(this)->SupportsInterface(riid))
			{
				*object = static_cast<Interface*>(this);
				AddRef();
				return S_OK;
			}
			return E_NOINTERFACE;
		}

		ULONG __stdcall AddRef() override { return ++References; }

		ULONG __stdcall Release() override
		{
			const ULONG remaining = --References;
			if (remaining == 0) delete static_cast<Derived*>(this);
			return remaining;
		}

		HRESULT __stdcall GetDevice(IDirect3DDevice9** device) override { return ReturnOwnerDevice(device); }
		HRESULT __stdcall SetPrivateData(REFGUID guid, const void* data, DWORD size, DWORD flags) override { return Shadow->SetPrivateData(guid, data, size, flags); }
		HRESULT __stdcall GetPrivateData(REFGUID guid, void* data, DWORD* size) override { return Shadow->GetPrivateData(guid, data, size); }
		HRESULT __stdcall FreePrivateData(REFGUID guid) override { return Shadow->FreePrivateData(guid); }

		DWORD __stdcall SetPriority(DWORD priority) override
		{
			const DWORD previous = Priority;
			Priority = priority;
			return previous;
		}

		DWORD __stdcall GetPriority() override { return Priority; }

		void __stdcall PreLoad() override
		{
			const HRESULT result = SyncAll("texture PreLoad");
			if (FAILED(result)) LogFailure("texture PreLoad upload", result);
			GPU->PreLoad();
		}

		D3DRESOURCETYPE __stdcall GetType() override { return Shadow->GetType(); }

		DWORD __stdcall SetLOD(DWORD lod) override
		{
			const DWORD previous = LOD;
			LOD = lod;
			return previous;
		}

		DWORD __stdcall GetLOD() override { return LOD; }
		DWORD __stdcall GetLevelCount() override { return Shadow->GetLevelCount(); }
		HRESULT __stdcall SetAutoGenFilterType(D3DTEXTUREFILTERTYPE filter) override { return GPU->SetAutoGenFilterType(filter); }
		D3DTEXTUREFILTERTYPE __stdcall GetAutoGenFilterType() override { return GPU->GetAutoGenFilterType(); }

		void __stdcall GenerateMipSubLevels() override
		{
			const HRESULT result = SyncAll("GenerateMipSubLevels");
			if (FAILED(result)) LogFailure("mipmap upload", result);
			GPU->GenerateMipSubLevels();
		}

		IDirect3DBaseTexture9* ApplicationTexture() override { return static_cast<IDirect3DBaseTexture9*>(static_cast<Interface*>(this)); }
		IDirect3DBaseTexture9* ShadowTexture() override { return static_cast<IDirect3DBaseTexture9*>(Shadow); }
		IDirect3DBaseTexture9* GPUTexture() override { return static_cast<IDirect3DBaseTexture9*>(GPU); }
		ULONG AddRefApplication() override { return AddRef(); }
		ULONG ReleaseApplication() override { return Release(); }
		HRESULT QueryApplicationInterface(REFIID riid, void** object) override { return QueryInterface(riid, object); }

		HRESULT ReturnOwnerDevice(IDirect3DDevice9** device) override
		{
			if (!device) return E_POINTER;
			*device = Owner;
			Owner->AddRef();
			return S_OK;
		}

		HRESULT SyncAll(const char* reason) override
		{
			if (!NeedsFullUpload.exchange(false))
			{
				return S_OK;
			}

			// UpdateTexture only copies regions which Direct3D considers dirty.
			// Locks performed through a texture level normally propagate that state,
			// but forcing the complete level-zero region dirty also covers explicit
			// subresource locks and D3DLOCK_NO_DIRTY_UPDATE.  Direct3D scales the
			// level-zero region through the remaining mip chain.
			const HRESULT dirtyResult = static_cast<Derived*>(this)->MarkShadowFullyDirty();
			if (FAILED(dirtyResult))
			{
				NeedsFullUpload = true;
				Logger::log << "[D3DManagedCompat] Failed to mark shadow dirty reason=" << reason
					<< " result=0x" << std::hex << static_cast<unsigned long>(dirtyResult)
					<< std::dec << std::endl;
				return dirtyResult;
			}

			const HRESULT result = RealDevice->UpdateTexture(ShadowTexture(), GPUTexture());
			if (FAILED(result))
			{
				NeedsFullUpload = true;
				Logger::log << "[D3DManagedCompat] UpdateTexture failed reason=" << reason
					<< " result=0x" << std::hex << static_cast<unsigned long>(result)
					<< std::dec << std::endl;
			}
			return result;
		}

		DWORD OriginalUsage() const override { return Usage; }

	protected:
		~ManagedTextureCommon()
		{
			UnregisterTexture(this);
			Shadow->Release();
			GPU->Release();
		}

		void MarkFullUploadRequired() { NeedsFullUpload = true; }
		IDirect3DDevice9* GetRealDevice() const { return RealDevice; }
		Interface* GetShadow() const { return Shadow; }
		Interface* GetGPU() const { return GPU; }

	private:
		std::atomic<ULONG> References{ 1 };
		IDirect3DDevice9* Owner;
		IDirect3DDevice9* RealDevice;
		Interface* Shadow;
		Interface* GPU;
		DWORD Usage;
		DWORD Priority = 0;
		DWORD LOD = 0;
		std::atomic<bool> NeedsFullUpload{ true };
	};

	class ManagedTexture2D final : public ManagedTextureCommon<ManagedTexture2D, IDirect3DTexture9>
	{
		using Base = ManagedTextureCommon<ManagedTexture2D, IDirect3DTexture9>;
	public:
		using Base::Base;
		bool SupportsInterface(REFIID riid) const { return SameGuid(riid, IID_Texture9Local); }

		HRESULT __stdcall GetLevelDesc(UINT level, D3DSURFACE_DESC* desc) override
		{
			const HRESULT result = GetShadow()->GetLevelDesc(level, desc);
			if (SUCCEEDED(result) && desc)
			{
				desc->Usage = OriginalUsage();
				desc->Pool = D3DPOOL_MANAGED;
			}
			return result;
		}

		HRESULT __stdcall GetSurfaceLevel(UINT level, IDirect3DSurface9** surface) override
		{
			if (!surface) return E_POINTER;
			*surface = nullptr;
			IDirect3DSurface9* shadowSurface = nullptr;
			IDirect3DSurface9* gpuSurface = nullptr;
			HRESULT result = GetShadow()->GetSurfaceLevel(level, &shadowSurface);
			if (FAILED(result)) return result;
			result = GetGPU()->GetSurfaceLevel(level, &gpuSurface);
			if (FAILED(result))
			{
				shadowSurface->Release();
				return result;
			}

			ManagedSurfaceLevel* wrapper = new (std::nothrow) ManagedSurfaceLevel(
				this, shadowSurface, gpuSurface, false, D3DCUBEMAP_FACE_POSITIVE_X, level);
			if (!wrapper)
			{
				shadowSurface->Release();
				gpuSurface->Release();
				return E_OUTOFMEMORY;
			}
			*surface = wrapper;
			return S_OK;
		}

		HRESULT __stdcall LockRect(UINT level, D3DLOCKED_RECT* locked, const RECT* rect, DWORD flags) override
		{
			const HRESULT result = GetShadow()->LockRect(level, locked, rect, flags);
			if (FAILED(result)) LogFailure("2D LockRect", result, level);
			return result;
		}

		HRESULT __stdcall UnlockRect(UINT level) override
		{
			const HRESULT result = GetShadow()->UnlockRect(level);
			if (FAILED(result))
			{
				LogFailure("2D UnlockRect", result, level);
				return result;
			}
			const HRESULT syncResult = SyncSurface(false, D3DCUBEMAP_FACE_POSITIVE_X, level);
			if (FAILED(syncResult)) LogFailure("2D level upload", syncResult, level);
			return result;
		}

		HRESULT __stdcall AddDirtyRect(const RECT* dirtyRect) override
		{
			MarkFullUploadRequired();
			return GetShadow()->AddDirtyRect(dirtyRect);
		}

		HRESULT SyncSurface(bool cube, D3DCUBEMAP_FACES, UINT level) override
		{
			if (cube) return D3DERR_INVALIDCALL;
			IDirect3DSurface9* source = nullptr;
			IDirect3DSurface9* destination = nullptr;
			HRESULT result = GetShadow()->GetSurfaceLevel(level, &source);
			if (FAILED(result)) return result;
			result = GetGPU()->GetSurfaceLevel(level, &destination);
			if (SUCCEEDED(result))
			{
				result = GetRealDevice()->UpdateSurface(source, nullptr, destination, nullptr);
			}
			source->Release();
			if (destination) destination->Release();
			if (FAILED(result)) MarkFullUploadRequired();
			return result;
		}

		HRESULT SyncVolume(UINT) override { return D3DERR_INVALIDCALL; }

		HRESULT MarkShadowFullyDirty()
		{
			return GetShadow()->AddDirtyRect(nullptr);
		}
	};

	class ManagedCubeTexture final : public ManagedTextureCommon<ManagedCubeTexture, IDirect3DCubeTexture9>
	{
		using Base = ManagedTextureCommon<ManagedCubeTexture, IDirect3DCubeTexture9>;
	public:
		using Base::Base;
		bool SupportsInterface(REFIID riid) const { return SameGuid(riid, IID_CubeTexture9Local); }

		HRESULT __stdcall GetLevelDesc(UINT level, D3DSURFACE_DESC* desc) override
		{
			const HRESULT result = GetShadow()->GetLevelDesc(level, desc);
			if (SUCCEEDED(result) && desc)
			{
				desc->Usage = OriginalUsage();
				desc->Pool = D3DPOOL_MANAGED;
			}
			return result;
		}

		HRESULT __stdcall GetCubeMapSurface(D3DCUBEMAP_FACES face, UINT level, IDirect3DSurface9** surface) override
		{
			if (!surface) return E_POINTER;
			*surface = nullptr;
			IDirect3DSurface9* shadowSurface = nullptr;
			IDirect3DSurface9* gpuSurface = nullptr;
			HRESULT result = GetShadow()->GetCubeMapSurface(face, level, &shadowSurface);
			if (FAILED(result)) return result;
			result = GetGPU()->GetCubeMapSurface(face, level, &gpuSurface);
			if (FAILED(result))
			{
				shadowSurface->Release();
				return result;
			}

			ManagedSurfaceLevel* wrapper = new (std::nothrow) ManagedSurfaceLevel(
				this, shadowSurface, gpuSurface, true, face, level);
			if (!wrapper)
			{
				shadowSurface->Release();
				gpuSurface->Release();
				return E_OUTOFMEMORY;
			}
			*surface = wrapper;
			return S_OK;
		}

		HRESULT __stdcall LockRect(D3DCUBEMAP_FACES face, UINT level, D3DLOCKED_RECT* locked, const RECT* rect, DWORD flags) override
		{
			const HRESULT result = GetShadow()->LockRect(face, level, locked, rect, flags);
			if (FAILED(result)) LogFailure("cube LockRect", result, level);
			return result;
		}

		HRESULT __stdcall UnlockRect(D3DCUBEMAP_FACES face, UINT level) override
		{
			const HRESULT result = GetShadow()->UnlockRect(face, level);
			if (FAILED(result))
			{
				LogFailure("cube UnlockRect", result, level);
				return result;
			}
			const HRESULT syncResult = SyncSurface(true, face, level);
			if (FAILED(syncResult)) LogFailure("cube level upload", syncResult, level);
			return result;
		}

		HRESULT __stdcall AddDirtyRect(D3DCUBEMAP_FACES face, const RECT* dirtyRect) override
		{
			MarkFullUploadRequired();
			return GetShadow()->AddDirtyRect(face, dirtyRect);
		}

		HRESULT SyncSurface(bool cube, D3DCUBEMAP_FACES face, UINT level) override
		{
			if (!cube) return D3DERR_INVALIDCALL;
			IDirect3DSurface9* source = nullptr;
			IDirect3DSurface9* destination = nullptr;
			HRESULT result = GetShadow()->GetCubeMapSurface(face, level, &source);
			if (FAILED(result)) return result;
			result = GetGPU()->GetCubeMapSurface(face, level, &destination);
			if (SUCCEEDED(result))
			{
				result = GetRealDevice()->UpdateSurface(source, nullptr, destination, nullptr);
			}
			source->Release();
			if (destination) destination->Release();
			if (FAILED(result)) MarkFullUploadRequired();
			return result;
		}

		HRESULT SyncVolume(UINT) override { return D3DERR_INVALIDCALL; }

		HRESULT MarkShadowFullyDirty()
		{
			for (unsigned int face = static_cast<unsigned int>(D3DCUBEMAP_FACE_POSITIVE_X);
				face <= static_cast<unsigned int>(D3DCUBEMAP_FACE_NEGATIVE_Z); ++face)
			{
				const HRESULT result = GetShadow()->AddDirtyRect(static_cast<D3DCUBEMAP_FACES>(face), nullptr);
				if (FAILED(result)) return result;
			}
			return S_OK;
		}
	};

	class ManagedVolumeTexture final : public ManagedTextureCommon<ManagedVolumeTexture, IDirect3DVolumeTexture9>
	{
		using Base = ManagedTextureCommon<ManagedVolumeTexture, IDirect3DVolumeTexture9>;
	public:
		using Base::Base;
		bool SupportsInterface(REFIID riid) const { return SameGuid(riid, IID_VolumeTexture9Local); }

		HRESULT __stdcall GetLevelDesc(UINT level, D3DVOLUME_DESC* desc) override
		{
			const HRESULT result = GetShadow()->GetLevelDesc(level, desc);
			if (SUCCEEDED(result) && desc)
			{
				desc->Usage = OriginalUsage();
				desc->Pool = D3DPOOL_MANAGED;
			}
			return result;
		}

		HRESULT __stdcall GetVolumeLevel(UINT level, IDirect3DVolume9** volume) override
		{
			if (!volume) return E_POINTER;
			*volume = nullptr;
			IDirect3DVolume9* shadowVolume = nullptr;
			IDirect3DVolume9* gpuVolume = nullptr;
			HRESULT result = GetShadow()->GetVolumeLevel(level, &shadowVolume);
			if (FAILED(result)) return result;
			result = GetGPU()->GetVolumeLevel(level, &gpuVolume);
			if (FAILED(result))
			{
				shadowVolume->Release();
				return result;
			}

			ManagedVolumeLevel* wrapper = new (std::nothrow) ManagedVolumeLevel(this, shadowVolume, gpuVolume, level);
			if (!wrapper)
			{
				shadowVolume->Release();
				gpuVolume->Release();
				return E_OUTOFMEMORY;
			}
			*volume = wrapper;
			return S_OK;
		}

		HRESULT __stdcall LockBox(UINT level, D3DLOCKED_BOX* locked, const D3DBOX* box, DWORD flags) override
		{
			const HRESULT result = GetShadow()->LockBox(level, locked, box, flags);
			if (FAILED(result)) LogFailure("volume LockBox", result, level);
			return result;
		}

		HRESULT __stdcall UnlockBox(UINT level) override
		{
			const HRESULT result = GetShadow()->UnlockBox(level);
			if (FAILED(result))
			{
				LogFailure("volume UnlockBox", result, level);
				return result;
			}
			const HRESULT syncResult = SyncVolume(level);
			if (FAILED(syncResult)) LogFailure("volume upload", syncResult, level);
			return result;
		}

		HRESULT __stdcall AddDirtyBox(const D3DBOX* dirtyBox) override
		{
			MarkFullUploadRequired();
			return GetShadow()->AddDirtyBox(dirtyBox);
		}

		HRESULT SyncSurface(bool, D3DCUBEMAP_FACES, UINT) override { return D3DERR_INVALIDCALL; }

		HRESULT SyncVolume(UINT) override
		{
			MarkFullUploadRequired();
			return SyncAll("volume level UnlockBox");
		}

		HRESULT MarkShadowFullyDirty()
		{
			return GetShadow()->AddDirtyBox(nullptr);
		}
	};

	inline HRESULT CreateManagedTexture2D(IDirect3DDevice9* owner, IDirect3DDevice9* realDevice,
		UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format,
		IDirect3DTexture9** output)
	{
		if (!output) return D3DERR_INVALIDCALL;
		*output = nullptr;
		IDirect3DTexture9* shadow = nullptr;
		IDirect3DTexture9* gpu = nullptr;
		HRESULT result = realDevice->CreateTexture(width, height, levels, usage, format, D3DPOOL_SYSTEMMEM, &shadow, nullptr);
		if (SUCCEEDED(result))
		{
			result = realDevice->CreateTexture(width, height, levels, usage, format, D3DPOOL_DEFAULT, &gpu, nullptr);
		}

		if (FAILED(result))
		{
			if (shadow) shadow->Release();
			if (gpu) gpu->Release();
			return result;
		}

		ManagedTexture2D* wrapper = new (std::nothrow) ManagedTexture2D(owner, realDevice, shadow, gpu, usage);
		if (!wrapper)
		{
			shadow->Release();
			gpu->Release();
			return E_OUTOFMEMORY;
		}
		RegisterTexture(wrapper);
		*output = wrapper;
		return S_OK;
	}

	inline HRESULT CreateManagedCubeTexture(IDirect3DDevice9* owner, IDirect3DDevice9* realDevice,
		UINT edge, UINT levels, DWORD usage, D3DFORMAT format, IDirect3DCubeTexture9** output)
	{
		if (!output) return D3DERR_INVALIDCALL;
		*output = nullptr;
		IDirect3DCubeTexture9* shadow = nullptr;
		IDirect3DCubeTexture9* gpu = nullptr;
		HRESULT result = realDevice->CreateCubeTexture(edge, levels, usage, format, D3DPOOL_SYSTEMMEM, &shadow, nullptr);
		if (SUCCEEDED(result))
		{
			result = realDevice->CreateCubeTexture(edge, levels, usage, format, D3DPOOL_DEFAULT, &gpu, nullptr);
		}

		if (FAILED(result))
		{
			if (shadow) shadow->Release();
			if (gpu) gpu->Release();
			return result;
		}

		ManagedCubeTexture* wrapper = new (std::nothrow) ManagedCubeTexture(owner, realDevice, shadow, gpu, usage);
		if (!wrapper)
		{
			shadow->Release();
			gpu->Release();
			return E_OUTOFMEMORY;
		}
		RegisterTexture(wrapper);
		*output = wrapper;
		return S_OK;
	}

	inline HRESULT CreateManagedVolumeTexture(IDirect3DDevice9* owner, IDirect3DDevice9* realDevice,
		UINT width, UINT height, UINT depth, UINT levels, DWORD usage, D3DFORMAT format,
		IDirect3DVolumeTexture9** output)
	{
		if (!output) return D3DERR_INVALIDCALL;
		*output = nullptr;
		IDirect3DVolumeTexture9* shadow = nullptr;
		IDirect3DVolumeTexture9* gpu = nullptr;
		HRESULT result = realDevice->CreateVolumeTexture(width, height, depth, levels, usage, format, D3DPOOL_SYSTEMMEM, &shadow, nullptr);
		if (SUCCEEDED(result))
		{
			result = realDevice->CreateVolumeTexture(width, height, depth, levels, usage, format, D3DPOOL_DEFAULT, &gpu, nullptr);
		}

		if (FAILED(result))
		{
			if (shadow) shadow->Release();
			if (gpu) gpu->Release();
			return result;
		}

		ManagedVolumeTexture* wrapper = new (std::nothrow) ManagedVolumeTexture(owner, realDevice, shadow, gpu, usage);
		if (!wrapper)
		{
			shadow->Release();
			gpu->Release();
			return E_OUTOFMEMORY;
		}
		RegisterTexture(wrapper);
		*output = wrapper;
		return S_OK;
	}

	inline IDirect3DBaseTexture9* PrepareForBinding(IDirect3DBaseTexture9* texture)
	{
		ManagedTextureControl* managed = FindApplicationTexture(texture);
		if (!managed) return texture;
		const HRESULT result = managed->SyncAll("SetTexture");
		if (FAILED(result)) LogFailure("SetTexture upload", result);
		return managed->GPUTexture();
	}

	inline IDirect3DBaseTexture9* PrepareUpdateSource(IDirect3DBaseTexture9* texture)
	{
		ManagedTextureControl* managed = FindApplicationTexture(texture);
		return managed ? managed->ShadowTexture() : texture;
	}

	inline IDirect3DBaseTexture9* PrepareUpdateDestination(IDirect3DBaseTexture9* texture)
	{
		ManagedTextureControl* managed = FindApplicationTexture(texture);
		return managed ? managed->GPUTexture() : texture;
	}

	inline bool ReturnApplicationTexture(IDirect3DBaseTexture9* gpuTexture, IDirect3DBaseTexture9** output)
	{
		ManagedTextureControl* managed = FindGPUTexture(gpuTexture);
		if (!managed) return false;
		gpuTexture->Release(); // Balance the reference returned by the real GetTexture.
		managed->AddRefApplication();
		*output = managed->ApplicationTexture();
		return true;
	}
}
