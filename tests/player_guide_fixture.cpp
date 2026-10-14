// Actual Guide builder + bundled font metrics; no D3D/headset required.
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <functional>
#include <iostream>
#include <cstdlib>
#include <cmath>
#include "HaloCEVR/UI/PlayerGuide.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "ThirdParty/stb/stb_truetype.h"
using D3DCOLOR=unsigned;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
struct Font{float lineHeight;stbtt_bakedchar cdata[96];};
struct UIRenderer{
    Font large{},medium{},small{};
    Font* GetLargeFont(){return &large;}Font* GetMediumFont(){return &medium;}Font* GetSmallFont(){return &small;}
    float CalculateStringWidth(const Font& font,const char* text){float result=0;for(unsigned char c:std::string(text)){CHECK(c>=32&&c<128);result+=font.cdata[c-32].xadvance;}return result;}
    void Init(const char* path){
        std::ifstream file(path,std::ios::binary);std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});CHECK(!bytes.empty());
        for(auto* font:{&large,&medium,&small}){
            const float size=font==&large?32:font==&medium?28:18;
            std::vector<unsigned char> bitmap(512*512);
            CHECK(stbtt_BakeFontBitmap(bytes.data(),0,size,bitmap.data(),512,512,32,96,font->cdata)>0);
            font->lineHeight=size;
        }
    }
};
struct Element{float x=0,y=0,w=0,h=0;bool bVisible=true;virtual ~Element()=default;};
struct UIPanel:Element{
    std::vector<Element*> children;
    UIPanel(float X,float Y,float W,float H,D3DCOLOR){x=X;y=Y;w=W;h=H;}
    void AddElement(Element* p){children.push_back(p);}
    ~UIPanel(){for(auto* p:children)delete p;}
};
struct UILabel:Element{
    std::string text;Font* font;
    UILabel(float X,float Y,std::string T,Font* F,D3DCOLOR):text(T),font(F){x=X;y=Y;h=font->lineHeight;}
};
struct UITextButton:Element{
    std::string text;Font* font;std::function<void()> onClick;
    UITextButton(float X,float Y,float W,float H,const char* T,Font* F,D3DCOLOR,D3DCOLOR,D3DCOLOR):text(T),font(F){x=X;y=Y;w=W;h=H;}
};
struct Game{static Game instance;UIRenderer renderer;UIRenderer* uiRenderer=&renderer;};Game Game::instance;
struct SettingsMenu{
    static constexpr D3DCOLOR lightPanelColour=0,textLabelColour=1,textValueColour=2,darkPanelColour=3,mediumPanelColour=4;
    int guidePage=0;std::vector<UIPanel*> guidePages;
    void PopulateGuideTab(UIPanel*);
    void NextPage(std::vector<UIPanel*>&,int&);void PrevPage(std::vector<UIPanel*>&,int&);void SetPage(std::vector<UIPanel*>&,int&);
};
// INSERT_PRODUCTION_GUIDE
int main(int argc,char** argv){
    CHECK(argc==2);auto& renderer=Game::instance.renderer;renderer.Init(argv[1]);
    UIPanel panel(0,0,640,420-32*1.1f,0);SettingsMenu menu;menu.PopulateGuideTab(&panel);CHECK(menu.guidePages.size()==StylishGuide::Pages.size());
    std::ofstream layout("/tmp/halo-r130-guide-layout.tsv");
    for(unsigned n=0;n<menu.guidePages.size();++n){
        const auto* page=menu.guidePages[n];CHECK(page->bVisible==(n==0));unsigned buttons=0;
        for(auto* element:page->children){
            if(auto* label=dynamic_cast<UILabel*>(element)){
                const float width=renderer.CalculateStringWidth(*label->font,label->text.c_str());
                if(label->y<page->h-40 && label->y+label->h>page->h-48)std::cerr<<"Guide page "<<n+1<<" overflow: "<<label->text<<" y="<<label->y<<"\n";
                CHECK(label->x+width<=page->w-20);CHECK(label->y+label->h<=page->h-8);
                if(label->y<page->h-40)CHECK(label->y+label->h<=page->h-48);
                layout<<n<<'\t'<<label->x<<'\t'<<label->y<<'\t'<<label->h<<'\t'<<label->text<<'\n';
            }
            if(auto* button=dynamic_cast<UITextButton*>(element)){
                CHECK(button->x>=0&&button->x+button->w<=page->w);CHECK(button->y+button->h<=page->h);
                CHECK(renderer.CalculateStringWidth(*button->font,button->text.c_str())<=button->w);++buttons;
                menu.guidePage=static_cast<int>(n);button->onClick();
                CHECK(menu.guidePage==static_cast<int>((n+(button->text=="Next"?1:StylishGuide::Pages.size()-1))%StylishGuide::Pages.size()));
                for(unsigned p=0;p<menu.guidePages.size();++p)CHECK(menu.guidePages[p]->bVisible==(static_cast<int>(p)==menu.guidePage));
            }
        }CHECK(buttons==2);menu.guidePage=0;menu.SetPage(menu.guidePages,menu.guidePage);
    }
    std::cout<<checks<<" production guide navigation and bundled-font layout checks passed\n";
}
