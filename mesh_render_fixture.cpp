// Compile the production mesh pass with an adversarial inherited DX9 state.
// This verifies state setup/projection use; it does not emulate the GPU.
#include <cstdint>
#include <vector>
#include <map>
#include <cstdlib>
#include <iostream>
using DWORD=std::uint32_t;using UINT=unsigned;
constexpr int FALSE=0,TRUE=1;
enum {
 D3DRS_CLIPPLANEENABLE,D3DRS_SCISSORTESTENABLE,D3DRS_FILLMODE,D3DRS_VERTEXBLEND,
 D3DRS_INDEXEDVERTEXBLENDENABLE,D3DRS_COLORWRITEENABLE,D3DRS_DEPTHBIAS,D3DRS_SLOPESCALEDEPTHBIAS,
 D3DRS_LIGHTING,D3DRS_FOGENABLE,D3DRS_ALPHABLENDENABLE,D3DRS_ALPHATESTENABLE,D3DRS_STENCILENABLE,
 D3DRS_CULLMODE,D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ZFUNC,D3DRS_SHADEMODE,
 D3DFILL_SOLID=50,D3DVBF_DISABLE,D3DTSS_COLOROP,D3DTSS_COLORARG1,D3DTSS_ALPHAOP,D3DTSS_ALPHAARG1,
 D3DTOP_SELECTARG1,D3DTOP_DISABLE,D3DTA_DIFFUSE,D3DCULL_NONE,D3DCMP_LESSEQUAL,D3DSHADE_GOURAUD,
 D3DFVF_XYZ=0x100,D3DFVF_DIFFUSE=0x200,D3DTS_WORLD,D3DTS_VIEW,D3DTS_PROJECTION,D3DPT_TRIANGLELIST
};
struct D3DMATRIX {float marker=0;};
#define FAILED(x) ((x)<0)
struct D3DVIEWPORT9{float MinZ=.2f,MaxZ=.25f;};
struct IDirect3DDevice9 {
 D3DVIEWPORT9 viewport;
 int GetViewport(D3DVIEWPORT9* v){*v=viewport;return 0;}
 void SetViewport(const D3DVIEWPORT9* v){viewport=*v;}
 std::map<int,DWORD> states;
 std::map<int,D3DMATRIX> transforms;
 unsigned draws=0,triangles=0;bool noShaders=false;
 void SetRenderState(int name,DWORD value){states[name]=value;}
 void SetVertexShader(void*){noShaders=true;}
 void SetPixelShader(void*){}
 void SetTexture(int,void*){}
 void SetTextureStageState(int,int,int){}
 void SetFVF(int){}
 void SetTransform(int name,const D3DMATRIX* m){transforms[name]=*m;}
 void DrawPrimitiveUP(int,UINT count,const void*,std::size_t){++draws;triangles+=count;}
};
class InGameRenderer {
public:
 struct VertexData3D {float x,y,z;DWORD color;};
 std::vector<VertexData3D> meshVertices;
 D3DMATRIX world{1},view{2},projection{3},meshProjection{4};
 void DrawMeshes(IDirect3DDevice9*);
};
// INSERT_PRODUCTION_MESH
unsigned checks=0;
#define CHECK(x)do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
int main(){
 InGameRenderer r;IDirect3DDevice9 d;
 r.DrawMeshes(&d);CHECK(d.draws==0 && d.states.empty());
 r.meshVertices.resize(3);
 for(int eye=0;eye<2;++eye){
  // The previous Halo pass left clipping/wireframe/blending and a different
  // depth projection. The solid glove must override each offending state.
  for(int state=D3DRS_CLIPPLANEENABLE;state<=D3DRS_SHADEMODE;++state)d.states[state]=99;
  d.transforms[D3DTS_PROJECTION]=r.projection;
  r.DrawMeshes(&d);
  CHECK(d.draws==unsigned(eye+1) && d.triangles==unsigned(eye+1));
  CHECK(d.transforms[D3DTS_PROJECTION].marker==r.meshProjection.marker);
  CHECK(d.states[D3DRS_FILLMODE]==D3DFILL_SOLID);
 CHECK(d.viewport.MinZ==0 && d.viewport.MaxZ==1);
  CHECK(d.states[D3DRS_CLIPPLANEENABLE]==0 && d.states[D3DRS_SCISSORTESTENABLE]==FALSE);
  CHECK(d.states[D3DRS_COLORWRITEENABLE]==15 && d.states[D3DRS_ALPHATESTENABLE]==FALSE);
  CHECK(d.states[D3DRS_ZENABLE]==TRUE && d.states[D3DRS_ZWRITEENABLE]==TRUE);
  CHECK(d.states[D3DRS_VERTEXBLEND]==D3DVBF_DISABLE && d.states[D3DRS_INDEXEDVERTEXBLENDENABLE]==FALSE);
 }
 std::cout<<checks<<" production solid-mesh render-state checks passed\n";
}
