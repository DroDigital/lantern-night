#pragma once
// Forward renderer: one lit shader (static + skinned variants), the 8 nearest point lights per
// draw (light 0 = the lantern, always included), fog, emissive, rim, hit flash and blob shadows,
// rendered into an HDR (MSAA) target and finished by a post pass: bloom, vignette, tonemapping.
#include <cstdint>
#include <string>
#include <vector>

#include "raylib.h"
#include "raymath.h"

namespace lk {

struct ModelAsset;
class Animator;

struct PointLight {
    Vector3 pos{};
    Vector3 color{1, 1, 1};   // linear, premultiplied by intensity
    float range = 5.0f;
};

struct RenderSettings {
    Vector3 ambient{0.02f, 0.025f, 0.035f};
    Vector3 fogColor{0.03f, 0.04f, 0.055f};
    Vector3 voidColor{0.004f, 0.006f, 0.01f};   // clear colour beyond the walls
    float lightFalloff = 1.0f;                  // quadratic falloff coefficient
    float fogDensity = 0.05f;
    float fogHeight = 0.0f;
    float fogHeightFalloff = 0.6f;
    Vector3 rimColor{0.10f, 0.14f, 0.22f};
    float rimStrength = 1.0f;
    float characterMinLight = 0.06f;
    float bloomThreshold = 1.0f;
    float bloomKnee = 0.5f;
    float bloomStrength = 0.8f;
    float exposure = 1.0f;
    float vignette = 0.4f;
    float vignetteRadius = 0.95f;
    Vector3 vignetteColor{0.0f, 0.0f, 0.01f};
    Vector3 shadowTint{0.2f, 0.35f, 0.6f};
    float saturation = 1.0f;
    int tonemapper = 0;   // 0 ACES, 1 AgX
    bool msaa = true;
    bool lanternShadows = false;   // omnidirectional shadow map for light 0 (the lantern)
    int shadowSize = 512;
};

struct DrawParams {
    Color tint = WHITE;
    Vector3 emissive{0, 0, 0};          // applied to all meshes
    Vector3 maskEmissive{0, 0, 0};      // applied only to meshes in emissiveMask
    uint64_t emissiveMask = 0;
    uint64_t hiddenMask = 0;            // meshes not drawn
    float hitFlash = 0.0f;
    bool character = false;             // apply minimum light + rim
    Vector3 lightProbe{0, 0, 0};        // position used to select lights (0 = model origin)
    bool hasProbe = false;
    float opacity = 1.0f;               // < 1 draws translucent (water)
};

class Renderer {
public:
    bool Init(int renderWidth, int renderHeight);
    void Shutdown();
    bool ReloadShaders();
    void Resize(int renderWidth, int renderHeight);

    RenderSettings& Settings() { return settings_; }
    Shader StaticShader() const { return lit_[0]; }
    Shader SkinnedShader() const { return lit_[1]; }

    // Lights for this frame. Light 0 should be the lantern (always bound).
    void SetLights(const std::vector<PointLight>& lights, bool firstIsLantern);
    const std::vector<PointLight>& Lights() const { return lights_; }

    void BeginScene(const Camera3D& cam);
    void SetTime(float t) { time_ = t; }
    void DrawModel(const ModelAsset& asset, const Matrix& world, const Animator* anim, const DrawParams& p);
    void DrawStaticMesh(const Mesh& mesh, const Material& mat, const Matrix& world, Vector3 probe, const DrawParams& p);
    void DrawBlobShadow(Vector3 pos, float radius, float strength);
    // Additive effects (call after opaque geometry).
    void DrawHalo(Vector3 pos, float size, Color color, float intensity);
    void DrawShaft(Vector3 apex, Vector3 dir, float length, float radius, Color color, float intensity);
    void DrawAdditiveBillboard(Texture2D tex, Vector3 pos, float size, Color color, float intensity);
    void EndScene();

    // Omnidirectional lantern shadow map. Call between frames (outside BeginScene):
    // for each face 0..5: BeginShadowFace(face) ... draw casters ... EndShadowFace().
    bool ShadowsReady() const { return shadowFbo_ != 0; }
    void BeginShadowPass(Vector3 lightPos, float farPlane);
    void BeginShadowFace(int face);
    void EndShadowFace();
    void EndShadowPass();
    bool InShadowPass() const { return inShadow_; }

    // Post pass to the current framebuffer (screen). screenFlash: 0..1 full-screen flash.
    void Present(int screenWidth, int screenHeight, float screenFlash = 0.0f, Color flashColor = WHITE);

    Texture2D RadialTexture() const { return radial_; }
    int DrawCalls() const { return drawCalls_; }
    const Camera3D& Cam() const { return cam_; }

private:
    struct LitLocs {
        int viewPos, ambient, lightCount, lightPos, lightColor, lightRange, fogColor, fogDensity, fogHeight,
            fogHeightFalloff, emissive, hitFlash, rimColor, rimStrength, minLight, boneMatrices, lightFalloff, opacity, time;
    };
    void BindLights(int which, Vector3 probe);
    void SetFrameUniforms(int which);
    bool CreateTargets(int w, int h);
    void DestroyTargets();

    RenderSettings settings_;
    Shader lit_[2]{};
    LitLocs locs_[2]{};
    Shader additive_{}, shaft_{}, bright_{}, blur_{}, composite_{};
    Shader shadow_[2]{};
    int shadowLocLight_[2]{-1, -1}, shadowLocFar_[2]{-1, -1}, shadowLocBones_ = -1;
    int litShadowCube_[2]{-1, -1}, litShadowOn_[2]{-1, -1}, litShadowFar_[2]{-1, -1};
    unsigned shadowFbo_ = 0, shadowCube_ = 0, shadowDepth_ = 0;
    int shadowSize_ = 0;
    bool inShadow_ = false, shadowValid_ = false, shadowFailed_ = false;
    Vector3 shadowLight_{};
    float shadowFar_ = 12.0f;
    void EnsureShadowTargets();
    void DestroyShadowTargets();
    int addIntensityLoc_ = -1, shaftIntensityLoc_ = -1;
    Texture2D radial_{};
    Mesh coneMesh_{};
    Material coneMat_{};

    std::vector<PointLight> lights_;
    bool firstIsLantern_ = false;
    Camera3D cam_{};
    int drawCalls_ = 0;
    float time_ = 0;

    int w_ = 0, h_ = 0;
    unsigned msaaFbo_ = 0, msaaColor_ = 0, msaaDepth_ = 0;
    RenderTexture2D hdr_{};       // resolved scene
    RenderTexture2D bloom_[4]{};  // half A/B, quarter A/B
    bool msaaActive_ = false;
};

// Helper: RGBA16F render texture (no depth unless requested).
RenderTexture2D LoadRenderTextureHDR(int w, int h, bool withDepth);
void UnloadRenderTextureHDR(RenderTexture2D& rt);

}  // namespace lk
