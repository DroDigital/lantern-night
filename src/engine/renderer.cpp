#include "engine/renderer.h"

#include <algorithm>
#include <cmath>

#include "engine/animator.h"
#include "engine/assets.h"
#include "engine/log.h"
#include "engine/paths.h"
#include "rlgl.h"

#if defined(_WIN32)
#define LK_GLAPI __stdcall
#else
#define LK_GLAPI
#endif

namespace lk {
namespace {
constexpr int kMaxLights = 8;
constexpr int kMaxBones = 64;
constexpr unsigned GL_RENDERBUFFER_ = 0x8D41;
constexpr unsigned GL_RGBA16F_ = 0x881A;
constexpr unsigned GL_DEPTH_COMPONENT24_ = 0x81A6;
constexpr int GL_COLOR_BUFFER_BIT_ = 0x4000;

using PFN_GenRenderbuffers = void(LK_GLAPI*)(int, unsigned*);
using PFN_BindRenderbuffer = void(LK_GLAPI*)(unsigned, unsigned);
using PFN_RenderbufferStorageMultisample = void(LK_GLAPI*)(unsigned, int, unsigned, int, int);
using PFN_DeleteRenderbuffers = void(LK_GLAPI*)(int, const unsigned*);

using PFN_GenTextures = void(LK_GLAPI*)(int, unsigned*);
using PFN_BindTexture = void(LK_GLAPI*)(unsigned, unsigned);
using PFN_TexImage2D = void(LK_GLAPI*)(unsigned, int, int, int, int, int, unsigned, unsigned, const void*);
using PFN_TexParameteri = void(LK_GLAPI*)(unsigned, unsigned, int);
PFN_GenTextures glGenTextures_ = nullptr;
PFN_BindTexture glBindTexture_ = nullptr;
PFN_TexImage2D glTexImage2D_ = nullptr;
PFN_TexParameteri glTexParameteri_ = nullptr;

// R32F cube map (raylib's rlLoadTextureCubemap rejects float formats on some drivers).
unsigned CreateFloatCubemap(int size) {
    if (!glGenTextures_) {
        glGenTextures_ = reinterpret_cast<PFN_GenTextures>(rlGetProcAddress("glGenTextures"));
        glBindTexture_ = reinterpret_cast<PFN_BindTexture>(rlGetProcAddress("glBindTexture"));
        glTexImage2D_ = reinterpret_cast<PFN_TexImage2D>(rlGetProcAddress("glTexImage2D"));
        glTexParameteri_ = reinterpret_cast<PFN_TexParameteri>(rlGetProcAddress("glTexParameteri"));
    }
    if (!glGenTextures_ || !glBindTexture_ || !glTexImage2D_ || !glTexParameteri_) return 0;
    constexpr unsigned kCube = 0x8513, kPosX = 0x8515, kR32F = 0x822E, kRed = 0x1903, kFloat = 0x1406;
    unsigned id = 0;
    glGenTextures_(1, &id);
    glBindTexture_(kCube, id);
    for (unsigned f = 0; f < 6; ++f) glTexImage2D_(kPosX + f, 0, int(kR32F), size, size, 0, kRed, kFloat, nullptr);
    glTexParameteri_(kCube, 0x2801, 0x2601);  // MIN_FILTER = LINEAR
    glTexParameteri_(kCube, 0x2800, 0x2601);  // MAG_FILTER = LINEAR
    for (unsigned w : {0x2802u, 0x2803u, 0x8072u}) glTexParameteri_(kCube, w, 0x812F);  // CLAMP_TO_EDGE
    glBindTexture_(kCube, 0);
    return id;
}

PFN_GenRenderbuffers glGenRenderbuffers_ = nullptr;
PFN_BindRenderbuffer glBindRenderbuffer_ = nullptr;
PFN_RenderbufferStorageMultisample glRenderbufferStorageMultisample_ = nullptr;
PFN_DeleteRenderbuffers glDeleteRenderbuffers_ = nullptr;

bool LoadGlExt() {
    glGenRenderbuffers_ = reinterpret_cast<PFN_GenRenderbuffers>(rlGetProcAddress("glGenRenderbuffers"));
    glBindRenderbuffer_ = reinterpret_cast<PFN_BindRenderbuffer>(rlGetProcAddress("glBindRenderbuffer"));
    glRenderbufferStorageMultisample_ =
        reinterpret_cast<PFN_RenderbufferStorageMultisample>(rlGetProcAddress("glRenderbufferStorageMultisample"));
    glDeleteRenderbuffers_ = reinterpret_cast<PFN_DeleteRenderbuffers>(rlGetProcAddress("glDeleteRenderbuffers"));
    return glGenRenderbuffers_ && glBindRenderbuffer_ && glRenderbufferStorageMultisample_ && glDeleteRenderbuffers_;
}

Shader LoadShaderChecked(const std::string& vs, const std::string& fs) {
    std::string vsPath = vs.empty() ? "" : paths::Asset("shaders/" + vs);
    std::string fsPath = paths::Asset("shaders/" + fs);
    Shader s = LoadShader(vs.empty() ? nullptr : vsPath.c_str(), fsPath.c_str());
    if (s.id == 0 || s.id == rlGetShaderIdDefault()) LK_ERROR("renderer: shader %s/%s failed to compile", vs.c_str(), fs.c_str());
    return s;
}

Mesh BuildCone(int segments) {
    Mesh m{};
    m.triangleCount = segments;
    m.vertexCount = segments * 3;
    m.vertices = static_cast<float*>(MemAlloc(unsigned(m.vertexCount * 3) * sizeof(float)));
    m.texcoords = static_cast<float*>(MemAlloc(unsigned(m.vertexCount * 2) * sizeof(float)));
    m.normals = static_cast<float*>(MemAlloc(unsigned(m.vertexCount * 3) * sizeof(float)));
    for (int i = 0; i < segments; ++i) {
        float a0 = float(i) / float(segments) * 2.0f * PI;
        float a1 = float(i + 1) / float(segments) * 2.0f * PI;
        float vx[3] = {0.0f, std::cos(a0), std::cos(a1)};
        float vy[3] = {0.0f, 1.0f, 1.0f};
        float vz[3] = {0.0f, std::sin(a0), std::sin(a1)};
        float u[3] = {(float(i) + 0.5f) / float(segments), float(i) / float(segments), float(i + 1) / float(segments)};
        for (int k = 0; k < 3; ++k) {
            int v = i * 3 + k;
            m.vertices[v * 3 + 0] = vx[k];
            m.vertices[v * 3 + 1] = vy[k];
            m.vertices[v * 3 + 2] = vz[k];
            m.texcoords[v * 2 + 0] = 0.5f;  // centre: shaft.fs edge fade uses x
            m.texcoords[v * 2 + 1] = vy[k];
            m.normals[v * 3 + 0] = vx[k];
            m.normals[v * 3 + 1] = 0.0f;
            m.normals[v * 3 + 2] = vz[k];
            (void)u;
        }
    }
    UploadMesh(&m, false);
    return m;
}
}  // namespace

RenderTexture2D LoadRenderTextureHDR(int w, int h, bool withDepth) {
    RenderTexture2D rt{};
    rt.id = rlLoadFramebuffer();
    if (rt.id == 0) return rt;
    rlEnableFramebuffer(rt.id);
    rt.texture.id = rlLoadTexture(nullptr, w, h, RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, 1);
    rt.texture.width = w;
    rt.texture.height = h;
    rt.texture.format = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
    rt.texture.mipmaps = 1;
    rlFramebufferAttach(rt.id, rt.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    if (withDepth) {
        rt.depth.id = rlLoadTextureDepth(w, h, true);
        rt.depth.width = w;
        rt.depth.height = h;
        rt.depth.format = 19;
        rt.depth.mipmaps = 1;
        rlFramebufferAttach(rt.id, rt.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
    }
    if (!rlFramebufferComplete(rt.id)) LK_ERROR("renderer: HDR framebuffer %dx%d incomplete", w, h);
    rlDisableFramebuffer();
    SetTextureFilter(rt.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(rt.texture, TEXTURE_WRAP_CLAMP);
    return rt;
}

void UnloadRenderTextureHDR(RenderTexture2D& rt) {
    if (rt.id == 0) return;
    UnloadRenderTexture(rt);
    rt = RenderTexture2D{};
}

bool Renderer::Init(int w, int h) {
    if (!ReloadShaders()) return false;
    Image img = GenImageGradientRadial(128, 128, 0.0f, WHITE, Color{255, 255, 255, 0});
    radial_ = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(radial_, TEXTURE_FILTER_BILINEAR);
    coneMesh_ = BuildCone(24);
    coneMat_ = LoadMaterialDefault();
    bool ext = LoadGlExt();
    if (!ext) LK_WARN("renderer: multisample renderbuffers unavailable; MSAA off");
    settings_.msaa = settings_.msaa && ext;
    return CreateTargets(w, h);
}

bool Renderer::ReloadShaders() {
    Shader fresh[2] = {LoadShaderChecked("lit_static.vs", "lit.fs"), LoadShaderChecked("lit_skinned.vs", "lit.fs")};
    Shader add = LoadShaderChecked("", "additive.fs");
    Shader shaft = LoadShaderChecked("", "shaft.fs");
    Shader bright = LoadShaderChecked("", "bright.fs");
    Shader blur = LoadShaderChecked("", "blur.fs");
    Shader comp = LoadShaderChecked("", "composite.fs");
    Shader sh0 = LoadShaderChecked("lit_static.vs", "shadow.fs");
    Shader sh1 = LoadShaderChecked("lit_skinned.vs", "shadow.fs");
    unsigned def = rlGetShaderIdDefault();
    bool ok = fresh[0].id != def && fresh[1].id != def && add.id != def && shaft.id != def && bright.id != def &&
              blur.id != def && comp.id != def && sh0.id != def && sh1.id != def;
    if (!ok) return false;
    for (auto* s : {&lit_[0], &lit_[1], &additive_, &shaft_, &bright_, &blur_, &composite_, &shadow_[0], &shadow_[1]})
        if (s->id != 0 && s->id != def) UnloadShader(*s);
    shadow_[0] = sh0;
    shadow_[1] = sh1;
    for (int i = 0; i < 2; ++i) {
        shadowLocLight_[i] = GetShaderLocation(shadow_[i], "shadowLightPos");
        shadowLocFar_[i] = GetShaderLocation(shadow_[i], "shadowFar");
        shadow_[i].locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shadow_[i], "matModel");
        shadow_[i].locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(shadow_[i], "matNormal");
    }
    shadowLocBones_ = GetShaderLocation(shadow_[1], "boneMatrices");
    shadow_[1].locs[SHADER_LOC_MATRIX_BONETRANSFORMS] = shadowLocBones_;
    lit_[0] = fresh[0];
    lit_[1] = fresh[1];
    additive_ = add;
    shaft_ = shaft;
    bright_ = bright;
    blur_ = blur;
    composite_ = comp;
    for (int i = 0; i < 2; ++i) {
        Shader& s = lit_[i];
        LitLocs& l = locs_[i];
        l.viewPos = GetShaderLocation(s, "viewPos");
        l.ambient = GetShaderLocation(s, "ambientColor");
        l.lightCount = GetShaderLocation(s, "lightCount");
        l.lightPos = GetShaderLocation(s, "lightPos");
        l.lightColor = GetShaderLocation(s, "lightColor");
        l.lightRange = GetShaderLocation(s, "lightRange");
        l.fogColor = GetShaderLocation(s, "fogColor");
        l.fogDensity = GetShaderLocation(s, "fogDensity");
        l.fogHeight = GetShaderLocation(s, "fogHeight");
        l.fogHeightFalloff = GetShaderLocation(s, "fogHeightFalloff");
        l.emissive = GetShaderLocation(s, "emissive");
        l.hitFlash = GetShaderLocation(s, "hitFlash");
        l.rimColor = GetShaderLocation(s, "rimColor");
        l.rimStrength = GetShaderLocation(s, "rimStrength");
        l.minLight = GetShaderLocation(s, "minLight");
        l.boneMatrices = GetShaderLocation(s, "boneMatrices");
        l.lightFalloff = GetShaderLocation(s, "lightFalloff");
        l.opacity = GetShaderLocation(s, "opacity");
        l.time = GetShaderLocation(s, "time");
        s.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(s, "matModel");
        s.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(s, "matNormal");
        if (i == 1) s.locs[SHADER_LOC_MATRIX_BONETRANSFORMS] = l.boneMatrices;
        litShadowCube_[i] = GetShaderLocation(s, "shadowCube");
        litShadowOn_[i] = GetShaderLocation(s, "shadowEnabled");
        litShadowFar_[i] = GetShaderLocation(s, "shadowFar");
    }
    addIntensityLoc_ = GetShaderLocation(additive_, "intensity");
    shaftIntensityLoc_ = GetShaderLocation(shaft_, "intensity");
    coneMat_.shader = shaft_;
    return true;
}

bool Renderer::CreateTargets(int w, int h) {
    w_ = std::max(w, 1);
    h_ = std::max(h, 1);
    hdr_ = LoadRenderTextureHDR(w_, h_, true);
    int hw = std::max(w_ / 2, 1), hh = std::max(h_ / 2, 1);
    int qw = std::max(w_ / 4, 1), qh = std::max(h_ / 4, 1);
    bloom_[0] = LoadRenderTextureHDR(hw, hh, false);
    bloom_[1] = LoadRenderTextureHDR(hw, hh, false);
    bloom_[2] = LoadRenderTextureHDR(qw, qh, false);
    bloom_[3] = LoadRenderTextureHDR(qw, qh, false);
    msaaActive_ = false;
    if (settings_.msaa) {
        msaaFbo_ = rlLoadFramebuffer();
        glGenRenderbuffers_(1, &msaaColor_);
        glBindRenderbuffer_(GL_RENDERBUFFER_, msaaColor_);
        glRenderbufferStorageMultisample_(GL_RENDERBUFFER_, 4, GL_RGBA16F_, w_, h_);
        glGenRenderbuffers_(1, &msaaDepth_);
        glBindRenderbuffer_(GL_RENDERBUFFER_, msaaDepth_);
        glRenderbufferStorageMultisample_(GL_RENDERBUFFER_, 4, GL_DEPTH_COMPONENT24_, w_, h_);
        glBindRenderbuffer_(GL_RENDERBUFFER_, 0);
        rlFramebufferAttach(msaaFbo_, msaaColor_, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_RENDERBUFFER, 0);
        rlFramebufferAttach(msaaFbo_, msaaDepth_, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
        msaaActive_ = rlFramebufferComplete(msaaFbo_);
        rlDisableFramebuffer();
        if (!msaaActive_) LK_WARN("renderer: MSAA framebuffer incomplete; MSAA off");
    }
    return hdr_.id != 0;
}

void Renderer::DestroyTargets() {
    UnloadRenderTextureHDR(hdr_);
    for (auto& b : bloom_) UnloadRenderTextureHDR(b);
    if (msaaFbo_) rlUnloadFramebuffer(msaaFbo_);
    if (msaaColor_ && glDeleteRenderbuffers_) glDeleteRenderbuffers_(1, &msaaColor_);
    if (msaaDepth_ && glDeleteRenderbuffers_) glDeleteRenderbuffers_(1, &msaaDepth_);
    msaaFbo_ = msaaColor_ = msaaDepth_ = 0;
    msaaActive_ = false;
}

void Renderer::Resize(int w, int h) {
    if (w == w_ && h == h_) return;
    DestroyTargets();
    CreateTargets(w, h);
}

void Renderer::Shutdown() {
    DestroyTargets();
    DestroyShadowTargets();
    for (auto* s : {&lit_[0], &lit_[1], &additive_, &shaft_, &bright_, &blur_, &composite_})
        if (s->id) UnloadShader(*s);
    if (radial_.id) UnloadTexture(radial_);
    coneMat_.shader = Shader{rlGetShaderIdDefault(), rlGetShaderLocsDefault()};
    UnloadMesh(coneMesh_);
}

void Renderer::SetLights(const std::vector<PointLight>& lights, bool firstIsLantern) {
    lights_ = lights;
    firstIsLantern_ = firstIsLantern;
}

void Renderer::SetFrameUniforms(int which) {
    Shader& s = lit_[which];
    const LitLocs& l = locs_[which];
    SetShaderValue(s, l.viewPos, &cam_.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(s, l.ambient, &settings_.ambient, SHADER_UNIFORM_VEC3);
    SetShaderValue(s, l.fogColor, &settings_.fogColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(s, l.fogDensity, &settings_.fogDensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, l.fogHeight, &settings_.fogHeight, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, l.fogHeightFalloff, &settings_.fogHeightFalloff, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, l.rimColor, &settings_.rimColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(s, l.lightFalloff, &settings_.lightFalloff, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, l.time, &time_, SHADER_UNIFORM_FLOAT);
    float one = 1.0f;
    SetShaderValue(s, l.opacity, &one, SHADER_UNIFORM_FLOAT);
    int on = (settings_.lanternShadows && shadowValid_) ? 1 : 0;
    int slot = 15;
    SetShaderValue(s, litShadowOn_[which], &on, SHADER_UNIFORM_INT);
    SetShaderValue(s, litShadowCube_[which], &slot, SHADER_UNIFORM_INT);
    SetShaderValue(s, litShadowFar_[which], &shadowFar_, SHADER_UNIFORM_FLOAT);
}

void Renderer::BindLights(int which, Vector3 probe) {
    // Lantern first, then the 7 (or 8) lights whose reach is closest to the probe.
    int idx[kMaxLights];
    int n = 0;
    int start = 0;
    if (firstIsLantern_ && !lights_.empty()) {
        idx[n++] = 0;
        start = 1;
    }
    struct Cand {
        float score;
        int i;
    };
    Cand cands[64];
    int nc = 0;
    for (int i = start; i < int(lights_.size()) && nc < 64; ++i) {
        float d = Vector3Distance(lights_[size_t(i)].pos, probe) - lights_[size_t(i)].range;
        cands[nc++] = {d, i};
    }
    int want = std::min(kMaxLights - n, nc);
    std::partial_sort(cands, cands + want, cands + nc, [](const Cand& a, const Cand& b) { return a.score < b.score; });
    for (int k = 0; k < want; ++k) idx[n++] = cands[k].i;

    Vector3 pos[kMaxLights], col[kMaxLights];
    float range[kMaxLights];
    for (int k = 0; k < n; ++k) {
        const PointLight& L = lights_[size_t(idx[k])];
        pos[k] = L.pos;
        col[k] = L.color;
        range[k] = L.range;
    }
    Shader& s = lit_[which];
    const LitLocs& l = locs_[which];
    SetShaderValue(s, l.lightCount, &n, SHADER_UNIFORM_INT);
    if (n > 0) {
        SetShaderValueV(s, l.lightPos, pos, SHADER_UNIFORM_VEC3, n);
        SetShaderValueV(s, l.lightColor, col, SHADER_UNIFORM_VEC3, n);
        SetShaderValueV(s, l.lightRange, range, SHADER_UNIFORM_FLOAT, n);
    }
}

void Renderer::BeginScene(const Camera3D& cam) {
    cam_ = cam;
    drawCalls_ = 0;
    RenderTexture2D target = hdr_;
    if (msaaActive_) {
        target.id = msaaFbo_;
    }
    BeginTextureMode(target);
    Color clear = {(unsigned char)std::clamp(settings_.voidColor.x * 255.0f, 0.0f, 255.0f),
                   (unsigned char)std::clamp(settings_.voidColor.y * 255.0f, 0.0f, 255.0f),
                   (unsigned char)std::clamp(settings_.voidColor.z * 255.0f, 0.0f, 255.0f), 255};
    ClearBackground(clear);
    BeginMode3D(cam);
    SetFrameUniforms(0);
    SetFrameUniforms(1);
    if (settings_.lanternShadows && shadowValid_) {
        rlActiveTextureSlot(15);
        rlEnableTextureCubemap(shadowCube_);
        rlActiveTextureSlot(0);
    }
}

void Renderer::DrawModel(const ModelAsset& asset, const Matrix& world, const Animator* anim, const DrawParams& p) {
    int which = asset.skinned ? 1 : 0;
    if (inShadow_) {
        Shader& ss = shadow_[which];
        if (asset.skinned && shadowLocBones_ >= 0) {
            static Matrix ident[kMaxBones];
            static bool init = false;
            if (!init) {
                for (auto& m : ident) m = MatrixIdentity();
                init = true;
            }
            const Matrix* mats = (anim && anim->Valid()) ? anim->SkinMatrices() : ident;
            rlEnableShader(ss.id);
            rlSetUniformMatrices(shadowLocBones_, mats, std::min(int(asset.model.skeleton.boneCount), kMaxBones));
        }
        for (int i = 0; i < asset.model.meshCount; ++i) {
            if (i < 64 && (p.hiddenMask >> i) & 1ull) continue;
            Material mat = asset.model.materials[asset.model.meshMaterial[i]];
            mat.shader = ss;
            DrawMesh(asset.model.meshes[i], mat, world);
        }
        return;
    }
    Shader& s = lit_[which];
    const LitLocs& l = locs_[which];
    Vector3 probe = p.hasProbe ? p.lightProbe : Vector3{world.m12, world.m13, world.m14};
    BindLights(which, probe);
    float minLight = p.character ? settings_.characterMinLight : 0.0f;
    float rim = p.character ? settings_.rimStrength : 0.0f;
    SetShaderValue(s, l.minLight, &minLight, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, l.rimStrength, &rim, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, l.hitFlash, &p.hitFlash, SHADER_UNIFORM_FLOAT);
    if (asset.skinned && l.boneMatrices >= 0) {
        static Matrix identity[kMaxBones];
        static bool init = false;
        if (!init) {
            for (auto& m : identity) m = MatrixIdentity();
            init = true;
        }
        const Matrix* mats = (anim && anim->Valid()) ? anim->SkinMatrices() : identity;
        int count = std::min(int(asset.model.skeleton.boneCount), kMaxBones);
        rlEnableShader(s.id);
        rlSetUniformMatrices(l.boneMatrices, mats, count);
    }
    const Model& m = asset.model;
    for (int i = 0; i < m.meshCount; ++i) {
        if (i < 64 && (p.hiddenMask >> i) & 1ull) continue;
        Vector3 em = p.emissive;
        if (i < 64 && (p.emissiveMask >> i) & 1ull) em = Vector3Add(em, p.maskEmissive);
        SetShaderValue(s, l.emissive, &em, SHADER_UNIFORM_VEC3);
        Material mat = m.materials[m.meshMaterial[i]];
        mat.shader = s;
        Color base = mat.maps[MATERIAL_MAP_DIFFUSE].color;
        mat.maps[MATERIAL_MAP_DIFFUSE].color = Color{(unsigned char)(base.r * p.tint.r / 255), (unsigned char)(base.g * p.tint.g / 255),
                                                     (unsigned char)(base.b * p.tint.b / 255), base.a};
        DrawMesh(m.meshes[i], mat, world);
        mat.maps[MATERIAL_MAP_DIFFUSE].color = base;
        ++drawCalls_;
    }
}

void Renderer::DrawStaticMesh(const Mesh& mesh, const Material& material, const Matrix& world, Vector3 probe,
                              const DrawParams& p) {
    if (inShadow_) {
        if (p.opacity < 1.0f) return;
        Material mat = material;
        mat.shader = shadow_[0];
        DrawMesh(mesh, mat, world);
        return;
    }
    Shader& s = lit_[0];
    const LitLocs& l = locs_[0];
    BindLights(0, probe);
    float zero = 0.0f;
    SetShaderValue(s, l.minLight, &zero, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, l.rimStrength, &zero, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, l.hitFlash, &p.hitFlash, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, l.emissive, &p.emissive, SHADER_UNIFORM_VEC3);
    SetShaderValue(s, l.opacity, &p.opacity, SHADER_UNIFORM_FLOAT);
    if (p.opacity < 1.0f) rlDisableDepthMask();
    Material mat = material;
    mat.shader = s;
    DrawMesh(mesh, mat, world);
    if (p.opacity < 1.0f) {
        rlDrawRenderBatchActive();
        rlEnableDepthMask();
        float one = 1.0f;
        SetShaderValue(s, l.opacity, &one, SHADER_UNIFORM_FLOAT);
    }
    ++drawCalls_;
}

void Renderer::DrawBlobShadow(Vector3 pos, float radius, float strength) {
    if (inShadow_) return;
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    rlSetTexture(radial_.id);
    rlBegin(RL_QUADS);
    unsigned char a = (unsigned char)std::clamp(strength * 255.0f, 0.0f, 255.0f);
    rlColor4ub(0, 0, 0, a);
    float y = pos.y + 0.03f;
    rlTexCoord2f(0, 0); rlVertex3f(pos.x - radius, y, pos.z - radius);
    rlTexCoord2f(0, 1); rlVertex3f(pos.x - radius, y, pos.z + radius);
    rlTexCoord2f(1, 1); rlVertex3f(pos.x + radius, y, pos.z + radius);
    rlTexCoord2f(1, 0); rlVertex3f(pos.x + radius, y, pos.z - radius);
    rlEnd();
    rlSetTexture(0);
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}

void Renderer::DrawAdditiveBillboard(Texture2D tex, Vector3 pos, float size, Color color, float intensity) {
    if (inShadow_) return;
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);
    BeginShaderMode(additive_);
    SetShaderValue(additive_, addIntensityLoc_, &intensity, SHADER_UNIFORM_FLOAT);
    DrawBillboard(cam_, tex, pos, size, color);
    EndShaderMode();
    EndBlendMode();
    rlEnableDepthMask();
}

void Renderer::DrawHalo(Vector3 pos, float size, Color color, float intensity) {
    DrawAdditiveBillboard(radial_, pos, size, color, intensity);
}

void Renderer::DrawShaft(Vector3 apex, Vector3 dir, float length, float radius, Color color, float intensity) {
    if (inShadow_) return;
    rlDrawRenderBatchActive();
    Vector3 d = Vector3Normalize(dir);
    Vector3 up = {0, 1, 0};
    Matrix rot = MatrixIdentity();
    float dot = Vector3DotProduct(up, d);
    if (dot < 0.9999f) {
        if (dot < -0.9999f) rot = MatrixRotateX(PI);
        else {
            Vector3 axis = Vector3Normalize(Vector3CrossProduct(up, d));
            rot = MatrixRotate(axis, std::acos(std::clamp(dot, -1.0f, 1.0f)));
        }
    }
    Matrix world = MatrixMultiply(MatrixMultiply(MatrixScale(radius, length, radius), rot), MatrixTranslate(apex.x, apex.y, apex.z));
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    BeginBlendMode(BLEND_ADDITIVE);
    coneMat_.maps[MATERIAL_MAP_DIFFUSE].color = color;
    SetShaderValue(shaft_, shaftIntensityLoc_, &intensity, SHADER_UNIFORM_FLOAT);
    DrawMesh(coneMesh_, coneMat_, world);
    EndBlendMode();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

void Renderer::EndScene() {
    if (shadowCube_) {
        rlActiveTextureSlot(15);
        rlDisableTextureCubemap();
        rlActiveTextureSlot(0);
    }
    shadowValid_ = false;
    EndMode3D();
    EndTextureMode();
    if (msaaActive_) {
        rlBindFramebuffer(RL_READ_FRAMEBUFFER, msaaFbo_);
        rlBindFramebuffer(RL_DRAW_FRAMEBUFFER, hdr_.id);
        rlBlitFramebuffer(0, 0, w_, h_, 0, 0, w_, h_, GL_COLOR_BUFFER_BIT_);
        rlDisableFramebuffer();
    }
}

void Renderer::EnsureShadowTargets() {
    int size = std::max(64, settings_.shadowSize);
    if ((shadowFbo_ && shadowSize_ == size) || shadowFailed_) return;
    DestroyShadowTargets();
    shadowSize_ = size;
    shadowCube_ = CreateFloatCubemap(size);
    shadowDepth_ = rlLoadTextureDepth(size, size, true);
    shadowFbo_ = rlLoadFramebuffer();
    rlFramebufferAttach(shadowFbo_, shadowDepth_, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
    rlFramebufferAttach(shadowFbo_, shadowCube_, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_CUBEMAP_POSITIVE_X, 0);
    if (!shadowCube_ || !rlFramebufferComplete(shadowFbo_)) {
        LK_WARN("renderer: lantern shadow framebuffer incomplete; shadows off");
        DestroyShadowTargets();
        shadowFailed_ = true;
    }
    rlDisableFramebuffer();
}

void Renderer::DestroyShadowTargets() {
    if (shadowFbo_) rlUnloadFramebuffer(shadowFbo_);
    if (shadowCube_) rlUnloadTexture(shadowCube_);
    shadowFbo_ = shadowCube_ = shadowDepth_ = 0;
}

void Renderer::BeginShadowPass(Vector3 lightPos, float farPlane) {
    EnsureShadowTargets();
    shadowLight_ = lightPos;
    shadowFar_ = std::max(1.0f, farPlane);
    for (int i = 0; i < 2; ++i) {
        SetShaderValue(shadow_[i], shadowLocLight_[i], &shadowLight_, SHADER_UNIFORM_VEC3);
        SetShaderValue(shadow_[i], shadowLocFar_[i], &shadowFar_, SHADER_UNIFORM_FLOAT);
    }
}

void Renderer::BeginShadowFace(int face) {
    if (!shadowFbo_) return;
    static const Vector3 dirs[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    static const Vector3 ups[6] = {{0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0}};
    rlFramebufferAttach(shadowFbo_, shadowCube_, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_CUBEMAP_POSITIVE_X + face, 0);
    RenderTexture2D rt{};
    rt.id = shadowFbo_;
    rt.texture.width = rt.texture.height = shadowSize_;
    BeginTextureMode(rt);
    ClearBackground(WHITE);  // 1.0 = nothing closer than the far plane
    Camera3D c{};
    c.position = shadowLight_;
    c.target = Vector3Add(shadowLight_, dirs[face]);
    c.up = ups[face];
    c.fovy = 90.0f;
    c.projection = CAMERA_PERSPECTIVE;
    BeginMode3D(c);
    inShadow_ = true;
}

void Renderer::EndShadowFace() {
    if (!shadowFbo_) return;
    rlDrawRenderBatchActive();
    EndMode3D();
    EndTextureMode();
    inShadow_ = false;
}

void Renderer::EndShadowPass() {
    inShadow_ = false;
    shadowValid_ = shadowFbo_ != 0;
}

static void Blit(const Texture2D& src, RenderTexture2D& dst) {
    DrawTexturePro(src, Rectangle{0, 0, float(src.width), -float(src.height)},
                   Rectangle{0, 0, float(dst.texture.width), float(dst.texture.height)}, Vector2{0, 0}, 0.0f, WHITE);
}

void Renderer::Present(int sw, int sh, float screenFlash, Color flashColor) {
    // Bright pass -> half res
    BeginTextureMode(bloom_[0]);
    ClearBackground(BLACK);
    BeginShaderMode(bright_);
    SetShaderValue(bright_, GetShaderLocation(bright_, "threshold"), &settings_.bloomThreshold, SHADER_UNIFORM_FLOAT);
    SetShaderValue(bright_, GetShaderLocation(bright_, "knee"), &settings_.bloomKnee, SHADER_UNIFORM_FLOAT);
    Blit(hdr_.texture, bloom_[0]);
    EndShaderMode();
    EndTextureMode();

    int dirLoc = GetShaderLocation(blur_, "direction");
    auto blurPass = [&](RenderTexture2D& src, RenderTexture2D& dst, Vector2 dir) {
        BeginTextureMode(dst);
        BeginShaderMode(blur_);
        Vector2 step = {dir.x / float(src.texture.width), dir.y / float(src.texture.height)};
        SetShaderValue(blur_, dirLoc, &step, SHADER_UNIFORM_VEC2);
        Blit(src.texture, dst);
        EndShaderMode();
        EndTextureMode();
    };
    blurPass(bloom_[0], bloom_[1], {1.5f, 0});
    blurPass(bloom_[1], bloom_[0], {0, 1.5f});
    blurPass(bloom_[0], bloom_[2], {2.0f, 0});
    blurPass(bloom_[2], bloom_[3], {0, 2.0f});
    blurPass(bloom_[3], bloom_[2], {2.5f, 0});
    blurPass(bloom_[2], bloom_[3], {0, 2.5f});

    // Composite to screen
    BeginShaderMode(composite_);
    auto setF = [&](const char* n, float v) { SetShaderValue(composite_, GetShaderLocation(composite_, n), &v, SHADER_UNIFORM_FLOAT); };
    auto setV = [&](const char* n, Vector3 v) { SetShaderValue(composite_, GetShaderLocation(composite_, n), &v, SHADER_UNIFORM_VEC3); };
    setF("bloomStrength", settings_.bloomStrength);
    setF("exposure", settings_.exposure);
    setF("vignette", settings_.vignette);
    setF("vignetteRadius", settings_.vignetteRadius);
    setV("vignetteColor", settings_.vignetteColor);
    setV("shadowTint", settings_.shadowTint);
    setF("saturation", settings_.saturation);
    setF("flash", screenFlash);
    setV("flashColor", Vector3{flashColor.r / 255.0f, flashColor.g / 255.0f, flashColor.b / 255.0f});
    int tm = settings_.tonemapper;
    SetShaderValue(composite_, GetShaderLocation(composite_, "tonemapper"), &tm, SHADER_UNIFORM_INT);
    SetShaderValueTexture(composite_, GetShaderLocation(composite_, "bloomA"), bloom_[0].texture);
    SetShaderValueTexture(composite_, GetShaderLocation(composite_, "bloomB"), bloom_[3].texture);
    DrawTexturePro(hdr_.texture, Rectangle{0, 0, float(w_), -float(h_)}, Rectangle{0, 0, float(sw), float(sh)}, Vector2{0, 0},
                   0.0f, WHITE);
    EndShaderMode();
}

}  // namespace lk
