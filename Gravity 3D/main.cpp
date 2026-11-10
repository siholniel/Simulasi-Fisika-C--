// Visualisasi gravitasi lubang hitam (Raylib).
// - Grid = lengkungan ruang (paraboloid Flamm dari metrik Schwarzschild)
// - Partikel = benda yang mengorbit; ada koreksi relativistik sehingga orbitnya berputar (presesi)
//
// Kontrol:
//   Drag mouse kiri : putar kamera        Scroll : zoom
//   SPACE : luncurkan 1 partikel          N : luncurkan 10 partikel      C : hapus semua
//   UP / DOWN : massa lubang hitam +/-    [ / ] : kecepatan waktu        P : pause
//   T : tampil/sembunyikan jejak

#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <random>
#include <vector>

// Satuan: GM = M, dan c^2 = 2, sehingga jari-jari Schwarzschild rs = 2GM/c^2 = M
static float M = 1.0f;
static const float C2 = 2.0f;
static const float GRID_R = 24.0f;
static const size_t TRAIL_MAX = 350;

struct Particle {
    float x, z, vx, vz;
    Color col;
    std::deque<Vector2> trail;
};

// Tinggi permukaan grid di jarak r dari pusat (paraboloid Flamm, edge luar = 0)
static float surfaceY(float r) {
    float rs = M;
    float R = std::max(GRID_R, rs * 1.01f);
    float rr = std::max(r, rs);
    return 2.0f * std::sqrt(rs * (rr - rs)) - 2.0f * std::sqrt(rs * (R - rs));
}

// Satu langkah integrasi. Suku (1 + 3h^2/(c^2 r^2)) meniru efek relativitas umum (presesi orbit).
static void step(Particle &p, float dt) {
    float r2 = p.x * p.x + p.z * p.z;
    float r = std::sqrt(r2);
    float h = p.x * p.vz - p.z * p.vx;  // momentum sudut per massa
    float k = -M / (r2 * r) * (1.0f + 3.0f * h * h / (C2 * r2));
    p.vx += k * p.x * dt;
    p.vz += k * p.z * dt;
    p.x += p.vx * dt;
    p.z += p.vz * dt;
}

int main() {
    InitWindow(1100, 750, "Gravitasi Lubang Hitam 3D");
    SetTargetFPS(60);

    std::mt19937 rng(std::random_device{}());
    auto rnd = [&](float a, float b) {
        return std::uniform_real_distribution<float>(a, b)(rng);
    };

    std::vector<Particle> ps;
    auto launch = [&]() {
        float r = rnd(7.0f, 18.0f);
        float a = rnd(0.0f, 2.0f * PI);
        float v = std::sqrt(M / r) * rnd(0.75f, 1.05f);
        Particle p;
        p.x = r * std::cos(a);
        p.z = r * std::sin(a);
        p.vx = -std::sin(a) * v;
        p.vz = std::cos(a) * v;
        p.col = ColorFromHSV(rnd(0.0f, 360.0f), 0.6f, 1.0f);
        ps.push_back(p);
    };
    for (int i = 0; i < 5; i++) launch();

    float yaw = 0.6f, pitch = 0.55f, dist = 38.0f;
    float timeScale = 1.0f;
    bool paused = false, showTrail = true;

    while (!WindowShouldClose()) {
        // ---- Input ----
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 d = GetMouseDelta();
            yaw -= d.x * 0.005f;
            pitch = std::clamp(pitch + d.y * 0.005f, -1.4f, 1.4f);
        }
        dist = std::clamp(dist * (1.0f - GetMouseWheelMove() * 0.08f), 8.0f, 90.0f);

        if (IsKeyPressed(KEY_SPACE)) launch();
        if (IsKeyPressed(KEY_N)) for (int i = 0; i < 10; i++) launch();
        if (IsKeyPressed(KEY_C)) ps.clear();
        if (IsKeyPressed(KEY_P)) paused = !paused;
        if (IsKeyPressed(KEY_T)) showTrail = !showTrail;
        if (IsKeyPressed(KEY_UP)) M = std::min(M + 0.25f, 3.0f);
        if (IsKeyPressed(KEY_DOWN)) M = std::max(M - 0.25f, 0.5f);
        if (IsKeyPressed(KEY_RIGHT_BRACKET)) timeScale = std::min(timeScale * 1.5f, 6.0f);
        if (IsKeyPressed(KEY_LEFT_BRACKET)) timeScale = std::max(timeScale / 1.5f, 0.1f);

        // ---- Simulasi ----
        if (!paused) {
            int steps = std::max(1, (int)(80 * timeScale));
            const float dt = 0.01f;
            for (auto &p : ps) {
                for (int s = 0; s < steps; s++) {
                    step(p, dt);
                    float r = std::sqrt(p.x * p.x + p.z * p.z);
                    if (r < M || r > GRID_R * 1.5f) { p.x = p.z = 1e9f; break; }  // ditandai mati
                }
                if (p.x < 1e8f) {
                    p.trail.push_back({p.x, p.z});
                    if (p.trail.size() > TRAIL_MAX) p.trail.pop_front();
                }
            }
            ps.erase(std::remove_if(ps.begin(), ps.end(), [](const Particle &p) { return p.x > 1e8f; }),
                     ps.end());
        }

        // ---- Kamera ----
        Camera3D cam = {};
        cam.position = {dist * std::cos(pitch) * std::sin(yaw), dist * std::sin(pitch) - 3.0f,
                        dist * std::cos(pitch) * std::cos(yaw)};
        cam.target = {0, -3.0f, 0};
        cam.up = {0, 1, 0};
        cam.fovy = 45.0f;
        cam.projection = CAMERA_PERSPECTIVE;

        // ---- Gambar ----
        BeginDrawing();
        ClearBackground(Color{5, 5, 12, 255});
        BeginMode3D(cam);

        // Grid polar: cincin (makin rapat ke pusat) dan jari-jari
        const int RINGS = 26, SEG = 96, SPOKES = 48, RADIAL = 70;
        float rs = M;
        auto ringR = [&](float t) { return rs + (GRID_R - rs) * t * t; };

        for (int i = 1; i <= RINGS; i++) {
            float r = ringR((float)i / RINGS);
            float glow = 1.0f - (float)i / RINGS;
            Color c = {(unsigned char)(40 + 80 * glow), (unsigned char)(90 + 100 * glow), 220, 160};
            float y = surfaceY(r);
            for (int j = 0; j < SEG; j++) {
                float a0 = 2.0f * PI * j / SEG, a1 = 2.0f * PI * (j + 1) / SEG;
                DrawLine3D({r * std::cos(a0), y, r * std::sin(a0)},
                           {r * std::cos(a1), y, r * std::sin(a1)}, c);
            }
        }
        for (int s = 0; s < SPOKES; s++) {
            float a = 2.0f * PI * s / SPOKES;
            for (int k = 0; k < RADIAL; k++) {
                float r0 = ringR((float)k / RADIAL), r1 = ringR((float)(k + 1) / RADIAL);
                DrawLine3D({r0 * std::cos(a), surfaceY(r0), r0 * std::sin(a)},
                           {r1 * std::cos(a), surfaceY(r1), r1 * std::sin(a)}, Color{40, 80, 160, 110});
            }
        }

        // Cincin acuan: horizon (merah), bola foton (oranye), ISCO (kuning)
        struct Guide { float r; Color c; };
        Guide guides[3] = {{rs, RED}, {1.5f * rs, ORANGE}, {3.0f * rs, YELLOW}};
        for (auto &g : guides)
            DrawCircle3D({0, surfaceY(g.r), 0}, g.r, {1, 0, 0}, 90.0f, g.c);
        DrawSphere({0, surfaceY(rs), 0}, rs * 0.9f, BLACK);

        // Partikel dan jejaknya
        for (auto &p : ps) {
            if (showTrail) {
                size_t n = p.trail.size();
                for (size_t i = 1; i < n; i++) {
                    Vector2 a = p.trail[i - 1], b = p.trail[i];
                    DrawLine3D({a.x, surfaceY(std::hypot(a.x, a.y)) + 0.05f, a.y},
                               {b.x, surfaceY(std::hypot(b.x, b.y)) + 0.05f, b.y},
                               Fade(p.col, (float)i / n));
                }
            }
            float r = std::hypot(p.x, p.z);
            DrawSphere({p.x, surfaceY(r) + 0.25f, p.z}, 0.25f, p.col);
        }
        EndMode3D();

        // ---- HUD ----
        char buf[160];
        std::snprintf(buf, sizeof buf, "Massa: %.2f   Partikel: %d   Waktu: x%.2f%s   FPS: %d", M,
                      (int)ps.size(), timeScale, paused ? "  [PAUSE]" : "", GetFPS());
        DrawText(buf, 15, 12, 20, RAYWHITE);
        DrawText("SPACE: 1 partikel  N: 10 partikel  C: hapus  UP/DOWN: massa  [ ]: waktu  P: pause  T: jejak",
                 15, 40, 15, GRAY);
        DrawText("Merah: horizon   Oranye: bola foton   Kuning: orbit stabil terdalam (ISCO)", 15, 60, 15,
                 GRAY);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}