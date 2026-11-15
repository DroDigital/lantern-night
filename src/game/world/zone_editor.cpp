#include "game/world/zone_editor.h"

#include <algorithm>
#include <cmath>

#include "engine/renderer.h"
#include "engine/ui.h"
#include "raymath.h"
#include "rlgl.h"

namespace lk {

void ZoneEditor::Open(const ZoneData& zone, Vector3 focus) {
    zone_ = zone;
    tilePalette_.clear();
    for (auto& [c, e] : zone_.legend)
        if (c != ' ') tilePalette_.push_back(c);
    tileIdx_ = std::min(tileIdx_, int(tilePalette_.size()) - 1);
    level_ = int(std::lround(focus.y / zone_.layerHeight));
    cam_.Reset(Vector3Add(focus, Vector3{0, 9.0f, 7.0f}), PI, -0.9f);
    selected_ = MarkerRef{};
    SetStatus("Editor: F2 to leave, Ctrl+S to save");
}

std::vector<Placement>* ZoneEditor::ListFor(int i) {
    switch (i) {
        case 0: return &zone_.enemies;
        case 1: return &zone_.shrines;
        case 2: return &zone_.braziers;
        case 3: return &zone_.items;
        case 4: return &zone_.gates;
        case 5: return &zone_.lights;
        case 6: return &zone_.hints;
        case 7: return &zone_.triggers;
        default: return nullptr;
    }
}

Vector3 ZoneEditor::MarkerPos(const MarkerRef& m) const {
    if (m.spawn) return Vector3{zone_.playerSpawn.x, zone_.playerSpawn.y, zone_.playerSpawn.z};
    if (!m.list || m.index < 0 || m.index >= int(m.list->size())) return Vector3{};
    const Vec3f& p = (*m.list)[size_t(m.index)].pos;
    return Vector3{p.x, p.y, p.z};
}

ZoneEditor::MarkerRef ZoneEditor::NearestMarker(Vector3 p, float maxDist) {
    MarkerRef best;
    float bd = maxDist;
    Vector3 sp{zone_.playerSpawn.x, zone_.playerSpawn.y, zone_.playerSpawn.z};
    if (Vector2Distance({sp.x, sp.z}, {p.x, p.z}) < bd && std::fabs(sp.y - p.y) < 1.0f) {
        bd = Vector2Distance({sp.x, sp.z}, {p.x, p.z});
        best = MarkerRef{nullptr, -1, true};
    }
    for (int li = 0; li < 8; ++li) {
        auto* list = ListFor(li);
        for (int i = 0; i < int(list->size()); ++i) {
            const Vec3f& q = (*list)[size_t(i)].pos;
            float d = Vector2Distance({q.x, q.z}, {p.x, p.z});
            if (d < bd && std::fabs(q.y - p.y) < 2.5f) {
                bd = d;
                best = MarkerRef{list, i, false};
            }
        }
    }
    return best;
}

bool ZoneEditor::Update(float dt, const json& cfg) {
    statusTime_ = std::max(0.0f, statusTime_ - dt);
    if (props_.empty()) props_ = cfg.value("props", std::vector<std::string>{});
    cam_.Update(dt);
    bool changed = false;
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

    if (IsKeyPressed(KEY_ONE)) mode_ = Mode::Tiles;
    if (IsKeyPressed(KEY_TWO)) mode_ = Mode::Props;
    if (IsKeyPressed(KEY_THREE)) mode_ = Mode::Markers;
    if (IsKeyPressed(KEY_PAGE_UP)) level_++;
    if (IsKeyPressed(KEY_PAGE_DOWN)) level_--;
    if (ctrl && IsKeyPressed(KEY_S)) save_ = true;

    float planeY = float(level_) * zone_.layerHeight;
    hoverValid_ = MouseOnPlane(cam_.Cam(), planeY, hover_);
    if (hoverValid_) zone_.WorldToCell(hover_.x, hover_.z, hoverCx_, hoverCz_);
    float wheel = GetMouseWheelMove();

    if (mode_ == Mode::Tiles && !tilePalette_.empty()) {
        if (wheel > 0) tileIdx_ = (tileIdx_ + 1) % int(tilePalette_.size());
        if (wheel < 0) tileIdx_ = (tileIdx_ + int(tilePalette_.size()) - 1) % int(tilePalette_.size());
        if (IsKeyPressed(KEY_R)) {
            // Rotate stairs: cycle through stairs legend entries.
            std::vector<char> stairs;
            for (char c : tilePalette_)
                if (zone_.Legend(c)->kind == TileKind::Stairs) stairs.push_back(c);
            auto it = std::find(stairs.begin(), stairs.end(), tilePalette_[size_t(tileIdx_)]);
            if (it != stairs.end()) {
                char next = stairs[size_t((it - stairs.begin() + 1) % stairs.size())];
                tileIdx_ = int(std::find(tilePalette_.begin(), tilePalette_.end(), next) - tilePalette_.begin());
            }
        }
        if (hoverValid_ && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            const ZoneLayer* L = zone_.Layer(level_);
            char cur = L ? L->At(hoverCx_, hoverCz_) : ' ';
            char want = tilePalette_[size_t(tileIdx_)];
            if (cur != want) {
                SetTile(zone_, level_, hoverCx_, hoverCz_, want);
                dirtyTiles_ = true;
            }
            painting_ = true;
        }
        if (hoverValid_ && (IsKeyDown(KEY_X) || IsKeyDown(KEY_DELETE))) {
            const ZoneLayer* L = zone_.Layer(level_);
            if (L && L->At(hoverCx_, hoverCz_) != ' ') {
                SetTile(zone_, level_, hoverCx_, hoverCz_, ' ');
                dirtyTiles_ = true;
            }
        }
        // Rebuild once the stroke ends (rebuilding per cell would stutter).
        bool strokeEnded = painting_ && !IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        if (strokeEnded) painting_ = false;
        if (dirtyTiles_ && !painting_ && !IsKeyDown(KEY_X) && !IsKeyDown(KEY_DELETE)) {
            dirtyTiles_ = false;
            changed = true;
        }
    } else if (mode_ == Mode::Props && !props_.empty()) {
        if (wheel > 0) propIdx_ = (propIdx_ + 1) % int(props_.size());
        if (wheel < 0) propIdx_ = (propIdx_ + int(props_.size()) - 1) % int(props_.size());
        int near = -1;
        float nd = 1.0f;
        for (int i = 0; i < int(zone_.props.size()); ++i) {
            const Vec3f& q = zone_.props[size_t(i)].pos;
            float d = Vector2Distance({q.x, q.z}, {hover_.x, hover_.z});
            if (d < nd && std::fabs(q.y - planeY) < 0.6f) {
                nd = d;
                near = i;
            }
        }
        if (hoverValid_ && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Placement p;
            p.type = "props";
            p.model = props_[size_t(propIdx_)];
            p.pos = Vec3f{std::round(hover_.x * 4) / 4, planeY, std::round(hover_.z * 4) / 4};
            p.extra = json{{"collide", true}};
            zone_.props.push_back(p);
            changed = true;
        }
        if (near >= 0 && IsKeyPressed(KEY_R)) {
            zone_.props[size_t(near)].rot = std::fmod(zone_.props[size_t(near)].rot + 15.0f, 360.0f);
            changed = true;
        }
        if (near >= 0 && (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_DELETE))) {
            zone_.props.erase(zone_.props.begin() + near);
            changed = true;
        }
    } else if (mode_ == Mode::Markers) {
        if (hoverValid_ && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            MarkerRef m = NearestMarker(hover_, 0.9f);
            bool sameAsSelected = m.spawn == selected_.spawn && m.list == selected_.list && m.index == selected_.index;
            if ((m.list || m.spawn) && !sameAsSelected) {
                selected_ = m;
            } else if (selected_.list || selected_.spawn) {
                Vec3f np{std::round(hover_.x * 4) / 4, planeY, std::round(hover_.z * 4) / 4};
                if (selected_.spawn) zone_.playerSpawn = np;
                else {
                    Vec3f& p = (*selected_.list)[size_t(selected_.index)].pos;
                    float dy = p.y - std::floor(p.y / zone_.layerHeight + 0.01f) * zone_.layerHeight;  // keep height offsets (torches)
                    p = Vec3f{np.x, np.y + dy, np.z};
                }
                changed = true;
            }
        }
        if ((selected_.list || selected_.spawn) && IsKeyPressed(KEY_R)) {
            float& rot = selected_.spawn ? zone_.playerYaw : (*selected_.list)[size_t(selected_.index)].rot;
            rot = std::fmod(rot + 45.0f, 360.0f);
            changed = true;
        }
        if (selected_.list && (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_DELETE))) {
            selected_.list->erase(selected_.list->begin() + selected_.index);
            selected_ = MarkerRef{};
            changed = true;
        }
        if (IsKeyPressed(KEY_N) && hoverValid_) {
            // New enemy marker of the next kind in the palette.
            auto kinds = cfg.value("enemy_kinds", std::vector<std::string>{"minion"});
            static size_t k = 0;
            Placement p;
            p.type = "enemies";
            p.model = kinds[k++ % kinds.size()];
            p.extra = json{{"kind", p.model}};
            p.id = "ed_" + std::to_string(zone_.enemies.size());
            p.pos = Vec3f{hover_.x, planeY, hover_.z};
            zone_.enemies.push_back(p);
            changed = true;
        }
    }
    return changed;
}

void ZoneEditor::DrawOverlay3D(Renderer& r) const {
    (void)r;
    rlDrawRenderBatchActive();
    float y = float(level_) * zone_.layerHeight + 0.05f;
    // Grid around the hovered cell.
    int cx = hoverCx_, cz = hoverCz_;
    float cs = zone_.cellSize;
    for (int i = -12; i <= 12; ++i) {
        DrawLine3D({float(cx - 12) * cs, y, float(cz + i) * cs}, {float(cx + 13) * cs, y, float(cz + i) * cs}, Color{90, 110, 140, 120});
        DrawLine3D({float(cx + i) * cs, y, float(cz - 12) * cs}, {float(cx + i) * cs, y, float(cz + 13) * cs}, Color{90, 110, 140, 120});
    }
    if (hoverValid_) {
        Vector3 c{(float(cx) + 0.5f) * cs, y + 0.02f, (float(cz) + 0.5f) * cs};
        DrawCubeWires(c, cs, 0.05f, cs, Color{255, 200, 90, 255});
    }
    auto marker = [&](Vec3f p, Color col, bool sel) {
        DrawSphere(Vector3{p.x, p.y + 0.4f, p.z}, sel ? 0.3f : 0.2f, col);
        DrawLine3D(Vector3{p.x, p.y, p.z}, Vector3{p.x, p.y + 0.4f, p.z}, col);
    };
    auto isSel = [&](const std::vector<Placement>* l, int i) { return selected_.list == l && selected_.index == i; };
    marker(zone_.playerSpawn, Color{80, 255, 120, 255}, selected_.spawn);
    const Color cols[8] = {{255, 70, 60, 255}, {255, 200, 90, 255}, {255, 130, 40, 255}, {120, 220, 255, 255},
                           {200, 200, 200, 255}, {255, 240, 160, 255}, {200, 120, 255, 255}, {120, 255, 220, 255}};
    for (int li = 0; li < 8; ++li) {
        auto* list = const_cast<ZoneEditor*>(this)->ListFor(li);
        for (int i = 0; i < int(list->size()); ++i) marker((*list)[size_t(i)].pos, cols[li], isSel(list, i));
    }
    rlDrawRenderBatchActive();
}

void ZoneEditor::DrawUi(Ui& ui) const {
    ui.Panel({20, ui.H() - 250, 820, 230}, Color{255, 255, 255, 220});
    const char* modes[] = {"Tiles", "Props", "Markers"};
    std::string line1 = std::string("EDITOR  [1] Tiles  [2] Props  [3] Markers   mode: ") + modes[int(mode_)] +
                        "   layer " + std::to_string(level_) + " (PgUp/PgDn)";
    ui.Text(line1, 40, ui.H() - 232, 22, Color{255, 220, 170, 255}, Align::Left, false);
    std::string item;
    if (mode_ == Mode::Tiles && !tilePalette_.empty()) {
        char c = tilePalette_[size_t(tileIdx_)];
        const LegendEntry* e = zone_.Legend(c);
        item = std::string("tile '") + c + "' " + (e ? (e->kind == TileKind::Wall ? "wall" : e->piece) : "") +
               (e && e->kind == TileKind::Stairs ? " rot " + std::to_string(e->rot) : "") +
               "   wheel: cycle  LMB: paint  X: erase  R: rotate stairs";
    } else if (mode_ == Mode::Props && !props_.empty()) {
        item = "prop " + props_[size_t(propIdx_)] + "   wheel: cycle  LMB: place  R: rotate  X: delete";
    } else {
        item = "LMB: select marker, LMB again: move  R: rotate  X: delete  N: new enemy";
    }
    ui.Text(item, 40, ui.H() - 196, 20, Color{220, 225, 230, 255}, Align::Left, false);
    ui.Text("Camera: hold RMB to look, WASD move, Q/E down/up, Shift fast.   Ctrl+S save   F2 leave", 40, ui.H() - 164, 20,
            Color{180, 185, 190, 255}, Align::Left, false);
    ui.Text(TextFormat("hover cell (%d, %d)", hoverCx_, hoverCz_), 40, ui.H() - 132, 20, Color{180, 185, 190, 255}, Align::Left, false);
    if (statusTime_ > 0) ui.Text(status_, 40, ui.H() - 92, 26, Color{140, 255, 160, 255}, Align::Left, false);
}

}  // namespace lk
