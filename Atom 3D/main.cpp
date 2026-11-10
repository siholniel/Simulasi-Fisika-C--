// Visualisasi orbital hidrogen 3D (point cloud Monte Carlo) pakai Raylib.
// Butuh C++17 + special math functions (GCC atau MSVC; Clang/libc++ belum support).
//
// Kontrol:
//   Mouse kiri (drag) : putar kamera     Scroll : zoom
//   Q / A : n naik / turun               W / S : l naik / turun
//   E / D : m naik / turun               = / - : jumlah titik +/- 10.000
//   R     : sampling ulang (acak lagi)

#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

// ---------- Fisika (satuan atom: jari-jari Bohr a0 = 1) ----------

// Fungsi gelombang radial R_nl(r)
static double radialR(int n, int l, double r) {
    double rho = 2.0 * r / n;
    double norm = std::sqrt(std::pow(2.0 / n, 3) * std::tgamma(n - l) /
                            (2.0 * n * std::tgamma(n + l + 1)));
    return norm * std::exp(-rho / 2.0) * std::pow(rho, l) *
           std::assoc_laguerre(n - l - 1, 2 * l + 1, rho);
}

// Bagian sudut (bentuk real dari harmonik sferis, tanpa normalisasi)
static double angular(int l, int m, double cosT, double phi) {
    int am = std::abs(m);
    double p = std::assoc_legendre(l, am, cosT);
    double f = (m == 0) ? 1.0 : (m > 0 ? std::cos(am * phi) : std::sin(am * phi));
    return p * f;
}

// ---------- Sampling titik ----------

struct Cloud {
    std::vector<Vector3> pos;
    std::vector<Color> col;
};

static Color heat(double t) {  // t: 0..1 -> biru gelap -> cyan -> kuning -> putih
    t = std::clamp(t, 0.0, 1.0);
    double r, g, b;
    if (t < 0.5) {
        double u = t / 0.5;
        r = 0.1 * u; g = 0.3 + 0.7 * u; b = 0.9;
    } else {
        double u = (t - 0.5) / 0.5;
        r = 0.1 + 0.9 * u; g = 1.0; b = 0.9 - 0.4 * u + 0.4 * u * u;
    }
    return Color{(unsigned char)(r * 255), (unsigned char)(g * 255),
                 (unsigned char)(std::clamp(b, 0.0, 1.0) * 255), 200};
}

static Cloud generate(int n, int l, int m, int count, std::mt19937 &rng) {
    std::uniform_real_distribution<double> U(0.0, 1.0);
    Cloud c;
    c.pos.reserve(count);
    c.col.reserve(count);

    // 1) CDF radial dari kerapatan r^2 * R^2 (inverse transform sampling)
    const double rmax = 4.0 * n * n + 8.0;
    const int N = 4000;
    const double dr = rmax / N;
    std::vector<double> cdf(N);
    double sum = 0.0;
    for (int i = 0; i < N; i++) {
        double r = (i + 0.5) * dr;
        double R = radialR(n, l, r);
        sum += r * r * R * R;
        cdf[i] = sum;
    }
    for (double &v : cdf) v /= sum;

    // 2) Maksimum bagian sudut (|cos/sin| <= 1, jadi cukup scan P_l^m saja)
    double ymax = 1e-12;
    for (int i = 0; i <= 400; i++) {
        double ct = -1.0 + 2.0 * i / 400.0;
        ymax = std::max(ymax, std::fabs(std::assoc_legendre(l, std::abs(m), ct)));
    }

    // 3) Ambil titik: r dari CDF, arah dari rejection sampling
    std::vector<double> dens;
    dens.reserve(count);
    double dmax = 0.0;
    while ((int)c.pos.size() < count) {
        double u = U(rng);
        size_t idx = std::lower_bound(cdf.begin(), cdf.end(), u) - cdf.begin();
        double r = (idx + U(rng)) * dr;

        double ct = 2.0 * U(rng) - 1.0;
        double phi = 2.0 * M_PI * U(rng);
        double y = angular(l, m, ct, phi);
        if (U(rng) > (y * y) / (ymax * ymax)) continue;  // tolak

        double st = std::sqrt(1.0 - ct * ct);
        // sumbu polar = sumbu Y (atas) di Raylib
        c.pos.push_back({(float)(r * st * std::cos(phi)), (float)(r * ct),
                         (float)(r * st * std::sin(phi))});
        double R = radialR(n, l, r);
        double d = R * R * y * y;
        dens.push_back(d);
        dmax = std::max(dmax, d);
    }
    for (double d : dens) c.col.push_back(heat(std::pow(d / dmax, 0.35)));
    return c;
}

// ---------- Main ----------

int main() {
    InitWindow(1100, 750, "Orbital Hidrogen 3D");
    SetTargetFPS(60);

    std::mt19937 rng(std::random_device{}());
    int n = 2, l = 1, m = 0, count = 40000;
    Cloud cloud = generate(n, l, m, count, rng);

    float yaw = 0.8f, pitch = 0.4f, dist = 4.0f * n * n + 4.0f;
    bool dirty = false;

    while (!WindowShouldClose()) {
        // Kamera orbit
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 d = GetMouseDelta();
            yaw -= d.x * 0.005f;
            pitch = std::clamp(pitch + d.y * 0.005f, -1.5f, 1.5f);
        }
        dist = std::max(2.0f, dist * (1.0f - GetMouseWheelMove() * 0.08f));

        // Ganti bilangan kuantum
        int on = n, ol = l, om = m, oc = count;
        if (IsKeyPressed(KEY_Q)) n = std::min(n + 1, 6);
        if (IsKeyPressed(KEY_A)) n = std::max(n - 1, 1);
        if (IsKeyPressed(KEY_W)) l++;
        if (IsKeyPressed(KEY_S)) l--;
        if (IsKeyPressed(KEY_E)) m++;
        if (IsKeyPressed(KEY_D)) m--;
        l = std::clamp(l, 0, n - 1);
        m = std::clamp(m, -l, l);
        if (IsKeyPressed(KEY_EQUAL)) count = std::min(count + 10000, 200000);
        if (IsKeyPressed(KEY_MINUS)) count = std::max(count - 10000, 5000);
        if (n != on) dist = 4.0f * n * n + 4.0f;
        if (n != on || l != ol || m != om || count != oc || IsKeyPressed(KEY_R)) dirty = true;

        if (dirty) {
            cloud = generate(n, l, m, count, rng);
            dirty = false;
        }

        Camera3D cam = {};
        cam.position = {dist * std::cos(pitch) * std::sin(yaw), dist * std::sin(pitch),
                        dist * std::cos(pitch) * std::cos(yaw)};
        cam.target = {0, 0, 0};
        cam.up = {0, 1, 0};
        cam.fovy = 45.0f;
        cam.projection = CAMERA_PERSPECTIVE;

        BeginDrawing();
        ClearBackground(Color{8, 8, 16, 255});
        BeginMode3D(cam);
        for (size_t i = 0; i < cloud.pos.size(); i++) DrawPoint3D(cloud.pos[i], cloud.col[i]);
        DrawSphere({0, 0, 0}, 0.15f, RED);  // inti
        EndMode3D();

        char buf[128];
        std::snprintf(buf, sizeof buf, "n=%d  l=%d  m=%d   titik=%d   FPS=%d", n, l, m, count,
                      GetFPS());
        DrawText(buf, 15, 12, 22, RAYWHITE);
        DrawText("Q/A: n   W/S: l   E/D: m   +/-: titik   R: acak ulang   drag: putar   scroll: zoom",
                 15, 42, 16, GRAY);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}