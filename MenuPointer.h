#pragma once
#include "InteractionModel.h"

namespace Immersion {
struct MenuRay { bool hit=false, valid=false, down=false; float u=0, v=0; };
struct MenuPoint { bool positioned=false, down=false; float x=0, y=0; };
class MenuPointer {
public:
    MenuPoint Update(const std::array<MenuRay,2>& rays) {
        std::array<Edges,2> edges{};
        std::array<bool,2> hits{};
        for (unsigned h=0; h<2; ++h) {
            const auto& r = rays[h];
            hits[h] = r.valid && r.hit && std::isfinite(r.u) && std::isfinite(r.v) &&
                r.u >= 0 && r.u <= 1 && r.v >= 0 && r.v <= 1;
            edges[h] = triggers[h].Update(r.down,r.valid);
        }
        if (capture >= 0 && (!hits[capture] || !edges[capture].down)) capture = -1;
        for (unsigned h=0; h<2; ++h) if (hits[h] && edges[h].pressed && capture < 0) {
            capture = static_cast<int>(h); owner = capture;
        }
        if (capture < 0 && !hits[owner] && hits[1-owner]) owner = 1-owner;
        if (!hits[owner]) return {};
        return {true,capture == owner && edges[owner].down,rays[owner].u,1.0f-rays[owner].v};
    }
    void Reset() { for (auto& button : triggers) button.Suspend(); capture = -1; owner = 1; }
private:
    std::array<Button,2> triggers;
    int owner=1, capture=-1;
};
}
