#include "HaloCEVR/Immersion/HandGripPose.h"
#include <iostream>
#include <cstdlib>
using namespace Immersion;
struct Bone {char BoneName[32]{};int Parent=-1;};
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
int main(){
    CHECK(HandBoneKey("frame l middlelow")==HandBoneKey("FRAME_L_MIDDLE LOW"));
    Bone bones[19]{};ModelNode rest[19]{};Transform bind[19]{};
    std::strcpy(bones[0].BoneName,"root");std::strcpy(bones[1].BoneName,"frame l wriste");bones[1].Parent=0;
    std::strcpy(bones[17].BoneName,"frame r wriste");bones[17].Parent=0;
    std::strcpy(bones[18].BoneName,"frame gun");bones[18].Parent=17;
    const char* fingers[]={"index","middle","ring","pinky","thumb"};
    const char* joints[]={"low","mid","tip"};
    const Matrix4 model=Matrix4().rotateX(23).rotateZ(-37).translate(.2f,-.3f,.1f);
    for(auto& t:bind)t=ModelTransform(model);
    for(unsigned f=0;f<5;++f)for(unsigned j=0;j<3;++j){
        const unsigned i=2+f*3+j;
        const auto name=std::string("frame l ")+fingers[f]+" "+joints[j];
        std::strcpy(bones[i].BoneName,name.c_str());bones[i].Parent=j?int(i-1):1;
        bind[i]=ModelTransform(model*Matrix4().translate(.04f+.016f*j,-.018f+.012f*f,0));
    }
    for(unsigned i=0;i<19;++i){std::strcpy(rest[i].name,bones[i].BoneName);rest[i].parent=bones[i].Parent;
        rest[i].inverseBind=ModelTransform(ModelMatrix(bind[i]).invertAffine());}
    // Real model/animation spelling difference must still resolve to bind pose.
    std::strcpy(rest[5].name,"frame l middlelow");
    for(float mirror:{-1.f,1.f})for(float yaw:{-70.f,30.f}){
        Transform out[19];const Matrix4 world=Matrix4().scale(1,mirror,1).rotateZ(yaw).translate(10,20,30);
        for(unsigned i=0;i<19;++i)out[i]=ModelTransform(world*ModelMatrix(bind[i]));
        const auto gun=out[18],main=out[17];
        Matrix4 desired=Matrix4().scale(1,mirror,1)*GripAxes({0,-1,0},{1,0,0},{0,0,1});desired.translate(4,5,6);
        PlacePalmGrip(bones,19,out,1,18,rest,19,desired,Matrix4());
        Matrix4 palm;CHECK(LeftPalmFrame(bones,19,out,1,palm));
        for(unsigned v=0;v<16;++v)CHECK(std::fabs(palm[v]-desired[v])<.0005f);
        const auto wrist=out[1];
        CloseGripHand(bones,19,out,1,18,rest,19);
        CHECK(std::memcmp(&gun,&out[18],sizeof(gun))==0);CHECK(std::memcmp(&main,&out[17],sizeof(main))==0);
        CHECK(std::memcmp(&wrist,&out[1],sizeof(wrist))==0);
        for(unsigned f=0;f<5;++f)for(unsigned j=1;j<3;++j){
            const unsigned i=2+f*3+j;
            CHECK(std::fabs((out[i].translation-out[i-1].translation).length()-.016f)<.0001f);
        }
        // Every non-thumb finger bends into the palm, not sideways or upwards.
        const auto inverse=Matrix4(desired).invertAffine();
        for(unsigned f=0;f<4;++f){const unsigned i=2+f*3;const Vector3 delta=inverse*out[i+1].translation-inverse*out[i].translation;
            CHECK(delta.z<-.012f);CHECK(std::fabs(delta.y)<.0001f);}
        // Reapplying from bind/current wrist is idempotent, including both eyes.
        Transform first[19];std::copy(std::begin(out),std::end(out),std::begin(first));
        CloseGripHand(bones,19,out,1,18,rest,19);
        for(unsigned i=0;i<19;++i)CHECK((first[i].translation-out[i].translation).length()<.0001f);
        // Shell grasp: four curled fingers, thumb stays in its extended bind
        // pose. The pad anchor follows hand rotation/reflection, not the gun.
        for(unsigned i=0;i<19;++i)out[i]=ModelTransform(world*ModelMatrix(bind[i]));
        CloseGripHand(bones,19,out,1,18,rest,19,1,0);
        for(unsigned i=14;i<=16;++i)CHECK((out[i].translation-world*bind[i].translation).length()<.0001f);
        for(unsigned f=0;f<4;++f){const unsigned i=2+f*3;
            CHECK((out[i+1].translation-world*bind[i+1].translation).length()>.01f);}
        Matrix4 pad;CHECK(ThumbPadFrame(bones,19,out,1,18,.012f,pad));
        Matrix4 palmNow;CHECK(LeftPalmFrame(bones,19,out,1,palmNow));
        const auto mid=(out[16].translation+out[15].translation)*.5f;
        CHECK((pad*Vector3()-mid+palmNow.getForwardAxis()*.012f).length()<.0001f);
        CHECK(pad.getLeftAxis().dot((out[16].translation-out[15].translation).normalize())>.999f);
        CHECK(pad.getDeterminant()*mirror>0);
        CloseGripHand(bones,19,out,1,18,rest,19,1,0);Matrix4 again;
        CHECK(ThumbPadFrame(bones,19,out,1,18,.012f,again));
        CHECK((pad*Vector3()-again*Vector3()).length()<.0001f);
    }
    // The grenade renderer uses the native right skeleton, not a reflected
    // copy of the left skeleton as weapon reloads do. Test both anatomical
    // names with proper (positive-determinant) wrist transforms.
    Bone rightBones[19];ModelNode rightRest[19];Transform right[19],left[19];
    const auto reflect=Matrix4().scale(1,-1,1);
    for(unsigned i=0;i<19;++i){
        rightBones[i]=bones[i];
        if(std::strstr(rightBones[i].BoneName,"frame l "))rightBones[i].BoneName[6]='r';
        else if(std::strstr(rightBones[i].BoneName,"frame r "))rightBones[i].BoneName[6]='l';
        right[i]=ModelTransform(reflect*ModelMatrix(bind[i])*reflect);left[i]=bind[i];
        rightRest[i]=rest[i];std::strcpy(rightRest[i].name,rightBones[i].BoneName);
        rightRest[i].inverseBind=ModelTransform(ModelMatrix(right[i]).invertAffine());
    }
    Matrix4 leftPalm,rightPalm;CHECK(LeftPalmFrame(bones,19,left,1,leftPalm));CHECK(LeftPalmFrame(rightBones,19,right,1,rightPalm));
    CHECK((rightPalm.getForwardAxis()-reflect*leftPalm.getForwardAxis()).length()<.0001f);
    CloseGripHand(bones,19,left,1,18,rest,19);
    CloseGripHand(rightBones,19,right,1,18,rightRest,19);
    for(unsigned i=1;i<17;++i){
        CHECK((right[i].translation-reflect*left[i].translation).length()<.0001f);
        CHECK(ModelMatrix(right[i]).getDeterminant()>.99f);
    }
    for(unsigned f=0;f<4;++f){const unsigned i=2+f*3;
        CHECK((right[i+1].translation-right[i].translation).dot(rightPalm.getForwardAxis())<-.012f);
    }
    std::cout<<checks<<" anatomical palm, finger-chain and mirrored grip checks passed\n";
}
