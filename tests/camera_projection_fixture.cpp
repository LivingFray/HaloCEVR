// Exercise the production overlay matrix extraction with deliberately different
// culling/raster frusta. This is a CPU fixture, not a D3D/headset simulation.
#include "HaloCEVR/Maths/Vectors.h"
#include "HaloCEVR/Immersion/RenderProjection.h"
#include <iostream>
#include <cstdlib>
struct D3DMATRIX{float _11=0,_12=0,_13=0,_14=0,_21=0,_22=0,_23=0,_24=0,_31=0,_32=0,_33=0,_34=0,_41=0,_42=0,_43=0,_44=0;};
struct Viewport{};
struct CameraFrustum{float fov=0;Vector3 position;};
struct Renderer{CameraFrustum frustum,frustum2;};
struct CameraRenderMatrices{Viewport viewport;struct{float rotation[9]{1,0,0,0,1,0,0,0,1};Vector3 translation;}viewMatrix;D3DMATRIX projectionMatrix;};
namespace Helpers{CameraRenderMatrices matrices;CameraRenderMatrices* GetActiveCameraMatrices(){return &matrices;}}
struct Game{static Game instance;void SetViewportScale(Viewport*){}};Game Game::instance;
namespace Hooks{const CameraFrustum* used=nullptr;
void SetCameraMatrices(Viewport*,CameraFrustum* camera,CameraRenderMatrices* out,bool){used=camera;
    out->viewMatrix.translation=-camera->position;
    auto& p=out->projectionMatrix;p={};p._11=1/std::tan(camera->fov/2);p._22=p._11;
    p._33=-1.001f;p._43=-.02001f;p._34=-1;
}}
class InGameRenderer{public:D3DMATRIX world,view,projection,meshProjection;void ExtractMatrices(Renderer*);};
// INSERT_PRODUCTION_EXTRACTION
unsigned checks=0;
#define CHECK(x)do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
int main(){
    for(int eye:{0,1}){
        Renderer camera;camera.frustum.fov=2.2f;camera.frustum2.fov=1.8f;
        camera.frustum2.position={2.f,eye?.012f:-.012f,1.2f};
        InGameRenderer overlay;overlay.ExtractMatrices(&camera);
        CHECK(Hooks::used==&camera.frustum2);
        CHECK(std::fabs(overlay.meshProjection._11-1/std::tan(.9f))<.00001f);
        CHECK(overlay.meshProjection._11==overlay.projection._11);
        CHECK(overlay.view._42==-camera.frustum2.position.y && overlay.view._43==-1.2f);
        CHECK(overlay.world._11==1 && overlay.world._22==1 && overlay.world._33==1 && overlay.world._44==1);
        const auto native=Immersion::FirstPersonDepth(Helpers::matrices.projectionMatrix);
        CHECK(overlay.meshProjection._33==native._33 && overlay.meshProjection._43==native._43);
    }
    std::cout<<checks<<" production stereo raster-projection and body-depth checks passed\n";
}
