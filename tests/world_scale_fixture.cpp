// Actual Game conversions and OpenVR per-eye frustum path, no headset needed.
#include "HaloCEVR/Maths/Matrices.h"
#include "HaloCEVR/Immersion/InteractionModel.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <iostream>
#include <cstdlib>
#define VR_PROFILE_SCOPE(x)
struct Property{float value=1;float Value()const{return value;}};
struct Game{static Game instance;struct Interaction{unsigned owner=0xffffffffu;bool ViewOverrideActive()const{return owner!=0xffffffffu;}void ApplyViewRotation(Matrix4&)const{}} interactions;Property* c_WorldScale=nullptr;float WorldScale()const;float MetresToWorld(float)const;float WorldToMetres(float)const;};
Game Game::instance;
// INSERT_PRODUCTION_CONVERSION
struct CameraFrustum{float fov=0;Vector3 position{},facingDirection{},upDirection{};};
struct CutsceneData{bool bInCutscene=false;};
namespace Helpers{CutsceneData scene;CutsceneData* GetCutsceneData(){return &scene;}}
namespace vr{enum Hmd_Eye{Left,Right};}
Matrix3 GetRotationMatrix(const Matrix4& m){Matrix3 r;for(unsigned i=0;i<3;++i)r.setColumn(i,&m.get()[i*4]);return r;}
class OpenVR{public:
 float fov=1.5f;Matrix4 head;
 Matrix4 GetHMDMatrixPoseEye(vr::Hmd_Eye eye){return Matrix4().translate(0,eye==vr::Left?-.032f:.032f,0);}
 Matrix4 GetHMDTransform(bool)const{return head;}
 void UpdateCameraFrustum(CameraFrustum*,int);
};
// INSERT_PRODUCTION_CAMERA
static unsigned checks=0;
#define CHECK(x)do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
int main(){
 auto& g=Game::instance;Property p;OpenVR vr;
 CHECK(g.MetresToWorld(3.048f)==1.f);g.c_WorldScale=&p;
 for(float scale:{.75f,.9f,1.f,1.1f,1.25f}){
  p.value=scale;CHECK(g.WorldScale()==scale);
  for(float metres:{-5.f,0.f,.064f,.2f,1.7f,10.f})CHECK(std::fabs(g.WorldToMetres(g.MetresToWorld(metres))-metres)<.00001f);
  vr.head=Matrix4().rotateZ(37).translate(.2f,.3f,1.7f);
  CameraFrustum a,b;a.position=b.position={10,20,30};
  vr.UpdateCameraFrustum(&a,0);vr.UpdateCameraFrustum(&b,1);
  CHECK(std::fabs((a.position-b.position).length()-.064f/(3.048f*scale))<.00001f);
  CHECK(std::fabs(a.fov-1.5f)<.00001f && b.fov==a.fov);
  const auto mean=(a.position+b.position)*.5f;
  CHECK((mean-Vector3(10,20,30)-Vector3(.2f,.3f,1.7f)/(3.048f*scale)).length()<.00001f);
  const auto original=a;vr.head.translate(.1f,0,0);a.position={10,20,30};vr.UpdateCameraFrustum(&a,0);
  CHECK((a.position-original.position-Vector3(.1f/(3.048f*scale),0,0)).length()<.00001f);
 }
 Helpers::scene.bInCutscene=true;g.interactions.owner=42;p.value=1;vr.head=Matrix4().rotateZ(25);
 CameraFrustum scripted;scripted.facingDirection={0,1,0};vr.UpdateCameraFrustum(&scripted,0);
 CHECK(scripted.facingDirection.dot(vr.head.getLeftAxis())>.9999f); // Passenger heading already applied once.
 g.interactions.owner=0xffffffffu;Helpers::scene.bInCutscene=false;
 p.value=0;CHECK(g.WorldScale()==.75f);p.value=20;CHECK(g.WorldScale()==1.25f);
 p.value=std::numeric_limits<float>::quiet_NaN();CHECK(g.WorldScale()==1);
 p.value=std::numeric_limits<float>::infinity();CHECK(g.WorldScale()==1);
 p.value=1;CHECK(g.WorldToMetres(1)==3.048f);
 std::cout<<checks<<" production world-scale, stereo separation and live headset translation checks passed\n";
}
