#include "HaloCEVR/Immersion/NativeModelPose.h"
#include <iostream>
#include <cstdlib>
#include <limits>
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
using namespace Immersion;
void bodyCases(){
    ModelNode nodes[13]{};Transform live[13]{},out[13]{},first[13]{};
    const char* names[]={"root","pelvis","spine","head","helmet","l upperarm","l wrist","r upperarm","r wrist","l thigh","l calf","r thigh","r calf"};
    const int parents[]={-1,0,1,2,3,2,5,2,7,1,9,1,11};
    for(unsigned n=0;n<13;++n){std::strcpy(nodes[n].name,names[n]);nodes[n].parent=parents[n];
        nodes[n].inverseBind=ModelTransform(Matrix4().translate({-.01f*float(n),0,-.1f*float(n)}));}
    for(float yaw:{0.f,90.f,179.f,-179.f}){
        const auto root=Matrix4().rotateZ(yaw).translate({10,20,30});
        for(unsigned n=0;n<13;++n)live[n]=ModelTransform(root*ModelMatrix(nodes[n].inverseBind).invertAffine());
        CHECK(PlaceOnFootBody(nodes,13,live,first));
        for(unsigned n=0;n<9;++n)CHECK(first[n].scale==0);
        for(unsigned n=9;n<13;++n)CHECK(first[n].scale==1);
        const auto before=live[10].translation;
        for(unsigned n=1;n<13;++n){
            const auto bind=ModelMatrix(nodes[n].inverseBind).invertAffine();
            live[n]=ModelTransform(root*Matrix4().rotateY(15).translate({0,0,.05f})*bind);
        }
        live[10].translation+=Vector3(.08f,0,0); // Independent walking leg swing.
        CHECK(PlaceOnFootBody(nodes,13,live,out));
        for(unsigned n:{0u,1u,2u})for(unsigned k=0;k<16;++k)CHECK(std::fabs(ModelMatrix(out[n])[k]-ModelMatrix(first[n])[k])<.00001f);
        CHECK((out[10].translation-first[10].translation).length()>.07f);
        CHECK((live[10].translation-before).length()>.01f); // Source remains animated.
    }
    nodes[2].parent=2;CHECK(!PlaceOnFootBody(nodes,13,live,out));nodes[2].parent=1;
    live[5].translation.x=std::numeric_limits<float>::quiet_NaN();CHECK(!PlaceOnFootBody(nodes,13,live,out));
    CHECK(!PlaceOnFootBody(nodes,65,live,out));CHECK(!PlaceOnFootBody(nodes,13,nullptr,out));
}
int main(){
    bodyCases();
    ModelNode nodes[7]{};
    const char* names[]={"frame","l arm","l wrist","l finger","r arm","r wrist","r finger"};
    const int parents[]={-1,0,1,2,0,4,5};
    for(int n=0;n<7;++n){std::strcpy(nodes[n].name,names[n]);nodes[n].parent=parents[n];
        Matrix4 bind;bind.translate({float(n),float(n)*.1f,0});nodes[n].inverseBind=ModelTransform(bind.invertAffine());}
    Transform placed[64]{};Matrix4 hands[2];hands[0].translate({4,5,6});hands[1].rotateZ(90).translate({7,8,9});
    CHECK(PlaceChiefHands(nodes,7,hands,placed));
    CHECK((placed[2].translation-Vector3(4,5,6)).length()<1e-5f);
    CHECK((placed[5].translation-Vector3(7,8,9)).length()<1e-5f);
    CHECK((placed[3].translation-Vector3(5,5.1f,6)).length()<1e-5f);
    CHECK(placed[1].scale==0 && placed[4].scale==0);CHECK(placed[2].scale==1 && placed[6].scale==1);
    // Independent hands keep the left wrist stable as the right controller moves.
    const auto before=placed[3].translation;hands[1].translate({3,4,5});CHECK(PlaceChiefHands(nodes,7,hands,placed));
    CHECK((placed[3].translation-before).length()<1e-5f);
    CHECK(PlaceModel(nodes,7,Matrix4().rotateY(90),placed));
    CHECK(std::abs(placed[3].translation.z+3)<1e-5f);
    nodes[2].parent=3;CHECK(!PlaceChiefHands(nodes,7,hands,placed));nodes[2].parent=1;
    nodes[6].parent=7;CHECK(!PlaceModel(nodes,7,hands[0],placed));nodes[6].parent=5;
    nodes[2].name[0]='x';CHECK(!PlaceChiefHands(nodes,7,hands,placed));nodes[2].name[0]='l';
    nodes[3].inverseBind.scale=0;CHECK(!PlaceModel(nodes,7,hands[0],placed));nodes[3].inverseBind.scale=1;
    nodes[0].inverseBind.translation.x=std::numeric_limits<float>::quiet_NaN();CHECK(!PlaceChiefHands(nodes,7,hands,placed));
    CHECK(!PlaceModel(nodes,65,hands[0],placed));CHECK(!PlaceModel(nullptr,7,hands[0],placed));
    std::cout<<checks<<" native model bind-pose/hand-placement checks passed\n";
}
