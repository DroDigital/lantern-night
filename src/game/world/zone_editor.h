#pragma once
// F2 level editor (debug builds). Free camera; pick a layer; place/rotate/delete tiles and
// props with the mouse; move/rotate/delete spawn markers; Ctrl+S saves data/levels/zone.json.
// Edits are applied by rebuilding the session from the edited ZoneData.
#include <string>
#include <vector>

#include "core/zone_data.h"
#include "engine/data_store.h"
#include "engine/editor.h"

namespace lk {

class Ui;
class Renderer;

class ZoneEditor {
public:
    void Open(const ZoneData& zone, Vector3 cameraFocus);
    // Returns true when the zone changed and the session should be rebuilt from Zone().
    bool Update(float dt, const json& editorCfg);
    void DrawOverlay3D(Renderer& r) const;
    void DrawUi(Ui& ui) const;
    const ZoneData& Zone() const { return zone_; }
    Camera3D Cam() const { return cam_.Cam(); }
    bool SaveRequested() { bool s = save_; save_ = false; return s; }
    void SetStatus(const std::string& s) { status_ = s; statusTime_ = 3.0f; }

private:
    enum class Mode { Tiles, Props, Markers };
    struct MarkerRef {
        std::vector<Placement>* list = nullptr;
        int index = -1;
        bool spawn = false;
    };
    std::vector<Placement>* ListFor(int i);
    Vector3 MarkerPos(const MarkerRef& m) const;
    MarkerRef NearestMarker(Vector3 p, float maxDist);

    ZoneData zone_;
    FreeCamera cam_;
    Mode mode_ = Mode::Tiles;
    int level_ = 0;
    std::vector<char> tilePalette_;
    int tileIdx_ = 0;
    int propIdx_ = 0;
    std::vector<std::string> props_;
    bool hoverValid_ = false;
    Vector3 hover_{};
    int hoverCx_ = 0, hoverCz_ = 0;
    MarkerRef selected_;
    bool painting_ = false, dirtyTiles_ = false;
    bool save_ = false;
    std::string status_;
    float statusTime_ = 0;
};

}  // namespace lk
