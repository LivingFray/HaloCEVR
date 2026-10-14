// Compile the production belt-label geometry and draw pass without a GPU.
// This verifies state setup/projection use; it does not emulate the GPU.
#include <cstdint>
#include <vector>
#include <map>
#include <cstdlib>
#include <iostream>
#include "HaloCEVR/Maths/Vectors.h"
#include "ThirdParty/stb/stb_truetype.h"
#define D3DCOLOR_ARGB(a,r,g,b) ((DWORD(a)<<24)|(DWORD(r)<<16)|(DWORD(g)<<8)|DWORD(b))
struct Font {void* fontBitmap=nullptr;stbtt_bakedchar cdata[96]{};float offset=0,lineHeight=0;};
using DWORD=std::uint32_t;using UINT=unsigned;
constexpr int FALSE=0,TRUE=1;
enum {
 D3DRS_CLIPPLANEENABLE,D3DRS_SCISSORTESTENABLE,D3DRS_FILLMODE,D3DRS_VERTEXBLEND,
 D3DRS_INDEXEDVERTEXBLENDENABLE,D3DRS_COLORWRITEENABLE,D3DRS_DEPTHBIAS,D3DRS_SLOPESCALEDEPTHBIAS,
 D3DRS_LIGHTING,D3DRS_FOGENABLE,D3DRS_ALPHABLENDENABLE,D3DRS_ALPHATESTENABLE,D3DRS_STENCILENABLE,
 D3DRS_CULLMODE,D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ZFUNC,D3DRS_SHADEMODE,
 D3DFILL_SOLID=50,D3DVBF_DISABLE,D3DTSS_COLOROP,D3DTSS_COLORARG1,D3DTSS_ALPHAOP,D3DTSS_ALPHAARG1,
 D3DTOP_SELECTARG1,D3DTOP_DISABLE,D3DTA_DIFFUSE,D3DCULL_NONE,D3DCMP_LESSEQUAL,D3DSHADE_GOURAUD,
 D3DRS_BLENDOP,D3DRS_SRCBLEND,D3DRS_DESTBLEND,D3DRS_SEPARATEALPHABLENDENABLE,D3DRS_SRGBWRITEENABLE,
 D3DBLENDOP_ADD,D3DBLEND_SRCALPHA,D3DBLEND_INVSRCALPHA,D3DTSS_ALPHAARG2,D3DTA_TEXTURE,
 D3DTSS_TEXCOORDINDEX,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE,D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,
 D3DSAMP_MIPFILTER,D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV,D3DTEXF_LINEAR,D3DTEXF_NONE,D3DTADDRESS_CLAMP,
 D3DTOP_SELECTARG2,D3DTOP_MODULATE,D3DFVF_TEX1=0x400,
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
 void SetSamplerState(int,int,int){}
 void SetFVF(int){}
 void SetTransform(int name,const D3DMATRIX* m){transforms[name]=*m;}
 void DrawPrimitiveUP(int,UINT count,const void*,std::size_t){++draws;triangles+=count;}
};
class InGameRenderer {
public:
 struct VertexData3D {float x,y,z;DWORD color;};
 std::vector<VertexData3D> meshVertices;
 D3DMATRIX world{1},view{2},projection{3},meshProjection{4};
 struct LabelVertex {float x,y,z;DWORD color;float u,v;};
 struct Label3D {void* texture=nullptr;bool respectDepth=false;std::vector<LabelVertex> background,glyphs;};
 std::vector<Label3D> labels;
 void DrawLabel3D(const Font&,const char*,const Vector3&,const Vector3&,const Vector3&,float,float=0,bool=false);
 void DrawLabels(IDirect3DDevice9*);
};
// INSERT_PRODUCTION_LABELS
unsigned checks=0;
#define CHECK(x)do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
int main(){
 InGameRenderer r; IDirect3DDevice9 d; Font font;
 const Vector3 center{1,2,3},right{0,-1,0},up{1,0,0};
 r.DrawLabels(&d);CHECK(d.draws==0 && d.states.empty());
 r.DrawLabel3D(font,"EMPTY",center,right,up,0.013f);CHECK(r.labels.empty());
 font.fontBitmap=reinterpret_cast<void*>(1);font.lineHeight=12;font.offset=9;
 for(auto& ch:font.cdata){ch.x0=10;ch.x1=16;ch.y0=10;ch.y1=19;ch.xoff=0;ch.yoff=-9;ch.xadvance=7;}
 r.DrawLabel3D(font,"EMPTY",center,right,up,0.013f,0.070f);CHECK(r.labels.size()==1);
 const auto& label=r.labels[0];CHECK(label.texture==font.fontBitmap);
 for(const auto& v:label.background) {
  CHECK(std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z));
  CHECK(std::fabs(v.z-3)<1e-6f);CHECK(std::fabs(v.y-2)<=0.03501f);
 }
 for(const auto& v:label.glyphs) {CHECK(v.u>=0 && v.u<=1 && v.v>=0 && v.v<=1);CHECK(v.z==3);}
 for(int eye=0;eye<2;++eye) {
  r.DrawLabels(&d);CHECK(d.draws==2*unsigned(eye+1));
  CHECK(d.states[D3DRS_ZENABLE]==FALSE && d.states[D3DRS_ZWRITEENABLE]==FALSE);
  CHECK(d.states[D3DRS_ALPHABLENDENABLE]==TRUE && d.states[D3DRS_SRCBLEND]==D3DBLEND_SRCALPHA);
  CHECK(d.states[D3DRS_STENCILENABLE]==FALSE && d.states[D3DRS_ALPHATESTENABLE]==FALSE);
  CHECK(d.transforms[D3DTS_PROJECTION].marker==r.projection.marker);
 }
 r.labels.clear();r.DrawLabel3D(font,"Magazine Zone",center,right,up,.012f,0,true);
 r.DrawLabels(&d);CHECK(d.states[D3DRS_ZENABLE]==TRUE);
 CHECK(d.transforms[D3DTS_PROJECTION].marker==r.meshProjection.marker);
 CHECK(d.states[D3DRS_ZFUNC]==D3DCMP_LESSEQUAL);
 CHECK(d.viewport.MinZ==0 && d.viewport.MaxZ==1);
 const auto size=r.labels.size();
 r.DrawLabel3D(font,"",center,right,up,0.013f);CHECK(r.labels.size()==size);
 r.DrawLabel3D(font,"EMPTY",center,right,up,0);CHECK(r.labels.size()==size);
 std::cout<<checks<<" production belt-label geometry and draw-state checks passed (no GPU)\n";
}
