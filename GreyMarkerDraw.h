#pragma once
// D3D9 types are provided by the caller. Change only the marker's individual
// draw calls; restore the real device directly so Halo's state cache stays valid.
template<class Device, class Shader> class GreyMarkerDraw {
public:
    GreyMarkerDraw(Device* device, Shader* shader) : device(device) {
        if (!shader || FAILED(device->GetPixelShader(&previous))) return;
        const bool read = SUCCEEDED(device->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend)) &&
            SUCCEEDED(device->GetRenderState(D3DRS_ALPHATESTENABLE,&alpha)) &&
            SUCCEEDED(device->GetRenderState(D3DRS_FOGENABLE,&fog));
        if (read && SUCCEEDED(device->SetPixelShader(shader))) {
            active=true;
            device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
            device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
            device->SetRenderState(D3DRS_FOGENABLE,FALSE);
        }
    }
    ~GreyMarkerDraw() {
        if (active) {
            device->SetPixelShader(previous);
            device->SetRenderState(D3DRS_ALPHABLENDENABLE,blend);
            device->SetRenderState(D3DRS_ALPHATESTENABLE,alpha);
            device->SetRenderState(D3DRS_FOGENABLE,fog);
        }
        if (previous) previous->Release();
    }
    GreyMarkerDraw(const GreyMarkerDraw&)=delete;
    GreyMarkerDraw& operator=(const GreyMarkerDraw&)=delete;
private:
    Device* device;
    Shader* previous=nullptr;
    DWORD blend=0,alpha=0,fog=0;
    bool active=false;
};
