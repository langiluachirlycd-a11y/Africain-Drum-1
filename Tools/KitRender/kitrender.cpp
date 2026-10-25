// DBL Drums - offline 3D render of the drum kit shown in the plugin (SDF ray marcher).
//   g++ -O3 -ffast-math -fopenmp -march=native kitrender.cpp -o kitrender
//   ./kitrender <width> <spp> <out.raw> <layout.inc>
// Output: linear float RGB (+ header w,h) and the click-zone layout used by DrumKitView.
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <string>
#include <omp.h>

using std::min;
using std::max;

struct V { float x = 0, y = 0, z = 0; };
static inline V mk (float x, float y, float z) { V v; v.x = x; v.y = y; v.z = z; return v; }
static inline V operator+ (V a, V b) { return mk (a.x + b.x, a.y + b.y, a.z + b.z); }
static inline V operator- (V a, V b) { return mk (a.x - b.x, a.y - b.y, a.z - b.z); }
static inline V operator* (V a, float s) { return mk (a.x * s, a.y * s, a.z * s); }
static inline V operator* (float s, V a) { return a * s; }
static inline V operator* (V a, V b) { return mk (a.x * b.x, a.y * b.y, a.z * b.z); }
static inline float dot (V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline float len (V a) { return sqrtf (dot (a, a)); }
static inline V norm (V a) { return a * (1.0f / (len (a) + 1e-12f)); }
static inline V cross (V a, V b) { return mk (a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static inline V refl (V v, V n) { return v - n * (2.0f * dot (v, n)); }
static inline float clampf (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline float mixf (float a, float b, float t) { return a + (b - a) * t; }
static inline V mixv (V a, V b, float t) { return a + (b - a) * t; }
static inline float sstep (float a, float b, float x) { float t = clampf ((x - a) / (b - a), 0.f, 1.f); return t * t * (3.f - 2.f * t); }
static inline V rotX (V p, float a) { float c = cosf (a), s = sinf (a); return mk (p.x, p.y * c - p.z * s, p.y * s + p.z * c); }
static inline V rotY (V p, float a) { float c = cosf (a), s = sinf (a); return mk (p.x * c + p.z * s, p.y, -p.x * s + p.z * c); }
static inline V rotZ (V p, float a) { float c = cosf (a), s = sinf (a); return mk (p.x * c - p.y * s, p.x * s + p.y * c, p.z); }

static float hash21 (float x, float y) { float h = sinf (x * 127.1f + y * 311.7f) * 43758.5453f; return h - floorf (h); }
static float vnoise (float x, float y)
{
    const float ix = floorf (x), iy = floorf (y), fx = x - ix, fy = y - iy;
    const float ux = fx * fx * (3 - 2 * fx), uy = fy * fy * (3 - 2 * fy);
    return mixf (mixf (hash21 (ix, iy), hash21 (ix + 1, iy), ux), mixf (hash21 (ix, iy + 1), hash21 (ix + 1, iy + 1), ux), uy);
}

// ------------------------------------------------------------------ decals
struct Tex
{
    int w = 0, h = 0;
    std::vector<float> d;

    bool load (const char* path)
    {
        FILE* f = fopen (path, "rb");
        if (! f) return false;
        if (fread (&w, 4, 1, f) != 1 || fread (&h, 4, 1, f) != 1) { fclose (f); return false; }
        d.resize ((size_t) w * h * 4);
        const size_t n = fread (d.data(), 4, d.size(), f);
        fclose (f);
        return n == d.size();
    }

    void sample (float u, float v, V& rgb, float& a) const
    {
        if (u < 0 || v < 0 || u > 1 || v > 1 || w == 0) { a = 0; return; }
        const float x = u * (w - 1), y = v * (h - 1);
        const int x0 = (int) x, y0 = (int) y, x1 = min (x0 + 1, w - 1), y1 = min (y0 + 1, h - 1);
        const float fx = x - x0, fy = y - y0;
        auto px = [&] (int xx, int yy, int c) { return d[((size_t) yy * w + xx) * 4 + c]; };
        float o[4];
        for (int c = 0; c < 4; ++c)
            o[c] = mixf (mixf (px (x0, y0, c), px (x1, y0, c), fx), mixf (px (x0, y1, c), px (x1, y1, c), fx), fy);
        rgb = mk (o[0], o[1], o[2]);
        a = o[3];
    }
};

static Tex texDbl, texSkm, texAg;

// ------------------------------------------------------------------ scene
enum { M_NONE = -1, M_FLOOR, M_CHROME, M_WOOD, M_HEAD, M_BRASS, M_BLACK, M_STEEL, M_KICKHEAD, M_WOODHOOP, M_BLACKNI };

struct Info { int mat = M_NONE; int obj = 0; V q; float aux = 0, aux2 = 0; };

struct Drum { V pos; float yaw, tilt, R, H; int obj, kind, lugs; };
struct Cym  { V pos; float yaw, tilt, tiltz, R, bell, bellH, sag; int obj; int holes = 0; };

static Drum drums[6];
static Cym cyms[10];

static float sdCylY (V q, float r, float h)
{
    const float dx = sqrtf (q.x * q.x + q.z * q.z) - r, dy = fabsf (q.y) - h;
    return min (max (dx, dy), 0.f) + sqrtf (max (dx, 0.f) * max (dx, 0.f) + max (dy, 0.f) * max (dy, 0.f));
}
static float sdBox (V p, V b)
{
    const V d = mk (fabsf (p.x) - b.x, fabsf (p.y) - b.y, fabsf (p.z) - b.z);
    return len (mk (max (d.x, 0.f), max (d.y, 0.f), max (d.z, 0.f))) + min (max (d.x, max (d.y, d.z)), 0.f);
}
static float sdCapsule (V p, V a, V b, float r)
{
    const V pa = p - a, ba = b - a;
    const float h = clampf (dot (pa, ba) / dot (ba, ba), 0.f, 1.f);
    return len (pa - ba * h) - r;
}

#define UPD(dist, m, o, qq, a1, a2) { const float _d = (dist); if (_d < best) { best = _d; if (info) { info->mat = (m); info->obj = (o); info->q = (qq); info->aux = (a1); info->aux2 = (a2); } } }

static float drumDist (V p, const Drum& D, float best, Info* info)
{
    const V q = rotX (rotY (p - D.pos, -D.yaw), -D.tilt);
    const float bound = len (q) - (D.R + D.H * 0.5f + 0.07f);
    if (bound > 0.09f) return min (best, bound);

    const float rr = sqrtf (q.x * q.x + q.z * q.z), hh = D.H * 0.5f;

    UPD (sdCylY (q, D.R - 0.0015f, hh - 0.0015f), M_WOOD, D.obj, q, D.R, D.H);

    for (int s = -1; s <= 1; s += 2)
    {
        V qh = q;
        qh.y = q.y - s * (hh + 0.003f);
        UPD (sdCylY (qh, D.R - 0.003f, 0.0025f), (D.kind == 2 && s == 1) ? M_KICKHEAD : M_HEAD, D.obj, q, D.R, D.H);

        if (D.kind == 2)
        {
            const float a = fabsf (rr - (D.R + 0.004f)) - 0.011f;
            const float b = fabsf (q.y - s * (hh + 0.006f)) - 0.019f;
            UPD (max (a, b), M_WOODHOOP, D.obj, q, D.R, D.H);
        }
        else
        {
            const float ry = rr - (D.R + 0.003f), yy = q.y - s * (hh + 0.007f);
            UPD (sqrtf (ry * ry + yy * yy) - 0.0078f, M_BLACKNI, D.obj, q, D.R, D.H);
        }
    }

    // lugs + tension rods (angular repetition)
    const float a = atan2f (q.z, q.x);
    const float sec = 6.2831853f / (float) D.lugs;
    const float aa = fmodf (a + 3.14159265f + sec * 0.5f, sec) - sec * 0.5f;
    const V lq = mk (rr * cosf (aa) - (D.R + 0.011f), fabsf (q.y) - (hh - 0.034f), rr * sinf (aa));
    UPD (sdBox (lq, mk (0.008f, 0.024f, 0.012f)), M_BLACKNI, D.obj, q, D.R, D.H);
    UPD (sdCylY (mk (lq.x, fabsf (q.y) - (hh - 0.018f), lq.z), 0.0032f, 0.036f), M_BLACKNI, D.obj, q, D.R, D.H);

    if (D.kind == 1)   // snare throw-off + butt plate
    {
        UPD (sdBox (q - mk (D.R + 0.02f, 0.0f, 0.0f), mk (0.016f, 0.024f, 0.03f)), M_BLACKNI, D.obj, q, D.R, D.H);
        UPD (sdBox (q - mk (-(D.R + 0.015f), 0.0f, 0.0f), mk (0.012f, 0.02f, 0.025f)), M_BLACKNI, D.obj, q, D.R, D.H);
    }

    return best;
}

static float cymDist (V p, const Cym& C, float best, Info* info)
{
    const V q = rotZ (rotX (rotY (p - C.pos, -C.yaw), -C.tilt), -C.tiltz);
    const float bound = len (q) - (C.R * 1.04f + 0.03f);
    if (bound > 0.06f) return min (best, bound);

    const float r = sqrtf (q.x * q.x + q.z * q.z);
    const float t = r / C.R;
    const float bellK = clampf (1.0f - r / C.bell, 0.f, 1.f);
    const float y = C.bellH * sqrtf (bellK) - C.sag * t * t;
    float d = (fabsf (q.y - y) - 0.0018f) * 0.55f;
    d = max (d, r - C.R);
    if (C.holes)
    {
        const float aa = atan2f (q.z, q.x);
        const int   rn[2]  = { 7, 12 };
        const float rr[2]  = { 0.42f, 0.74f };
        const float hrad[2] = { 0.105f, 0.085f };
        for (int k = 0; k < 2; ++k)
        {
            const float sec = 6.2831853f / (float) rn[k];
            float a2 = fmodf (aa + 6.2831853f, sec) - sec * 0.5f;
            const float hx = r * cosf (a2) - rr[k] * C.R, hz = r * sinf (a2);
            d = max (d, -(sqrtf (hx * hx + hz * hz) - hrad[k] * C.R) * 0.6f);
        }
    }
    UPD (d, M_BRASS, C.obj, q, C.R, 0.f);
    UPD (len (q - mk (0, C.bellH + 0.005f, 0)) - 0.011f, M_BLACK, C.obj, q, 0.f, 0.f);
    return best;
}

static const V kFootHH = mk (-0.88f, 0.0f, -0.02f);

static float map (V p, Info* info)
{
    float best = 1e9f;
    if (info) info->mat = M_NONE;

    UPD (p.y, M_FLOOR, 0, p, 0.f, 0.f);

    for (const auto& d : drums) best = drumDist (p, d, best, info);
    for (const auto& c : cyms)  best = cymDist (p, c, best, info);

    auto cap = [&] (V a, V b, float r, int m = M_CHROME) { UPD (sdCapsule (p, a, b, r), m, 20, p, 0.f, 0.f); };

    // tripod bases + poles
    auto tripod = [&] (V base, float topY, float spread, float poleR)
    {
        cap (mk (base.x, 0.11f, base.z), mk (base.x, topY, base.z), poleR);
        UPD (len (p - mk (base.x, 0.11f, base.z)) - 0.022f, M_BLACK, 20, p, 0.f, 0.f);
        for (int i = 0; i < 3; ++i)
        {
            const float ang = 1.5708f + (float) i * 2.0944f;
            const V foot = mk (base.x + cosf (ang) * spread, 0.013f, base.z + sinf (ang) * spread);
            cap (mk (base.x, 0.11f, base.z), foot, 0.0085f);
            UPD (len (p - foot) - 0.016f, M_BLACK, 20, p, 0.f, 0.f);
        }
    };

    // hi-hat
    tripod (kFootHH, 0.87f, 0.36f, 0.0105f);
    // pedal + footboard
    {
        const V c = mk (kFootHH.x, 0.035f, kFootHH.z + 0.20f);
        V q = rotX (p - c, 0.22f);
        UPD (sdBox (q, mk (0.052f, 0.010f, 0.115f)) - 0.004f, M_CHROME, 20, p, 0.f, 0.f);
        cap (mk (kFootHH.x, 0.045f, kFootHH.z + 0.08f), mk (kFootHH.x, 0.14f, kFootHH.z + 0.005f), 0.006f);
        UPD (sdBox (p - mk (kFootHH.x, 0.012f, kFootHH.z + 0.14f), mk (0.07f, 0.008f, 0.17f)), M_CHROME, 20, p, 0.f, 0.f);
    }

    // crash 1 (boom), crash 2 (boom), ride (boom)
    tripod (mk (-0.66f, 0, -0.34f), 1.20f, 0.40f, 0.0105f);
    cap (mk (-0.66f, 1.20f, -0.34f), mk (-0.62f, 1.345f, -0.40f), 0.0085f);
    tripod (mk (0.20f, 0, -0.60f), 1.24f, 0.36f, 0.0105f);
    cap (mk (0.20f, 1.24f, -0.60f), mk (0.16f, 1.325f, -0.47f), 0.0085f);
    tripod (mk (1.10f, 0, -0.16f), 1.02f, 0.40f, 0.0105f);
    cap (mk (1.10f, 1.02f, -0.16f), mk (1.05f, 1.105f, -0.22f), 0.0085f);
    // crash 2, crash 3, splash 2, china, FX (holes)
    tripod (mk (-0.34f, 0, -0.78f), 1.42f, 0.42f, 0.0105f);
    cap (mk (-0.34f, 1.42f, -0.78f), mk (-0.30f, 1.535f, -0.68f), 0.0085f);
    tripod (mk (0.66f, 0, -0.74f), 1.40f, 0.42f, 0.0105f);
    cap (mk (0.66f, 1.40f, -0.74f), mk (0.62f, 1.505f, -0.64f), 0.0085f);
    tripod (mk (-1.18f, 0, -0.30f), 0.98f, 0.34f, 0.0100f);
    cap (mk (-1.18f, 0.98f, -0.30f), mk (-1.13f, 1.09f, -0.22f), 0.0080f);
    tripod (mk (1.52f, 0, -0.38f), 0.98f, 0.38f, 0.0105f);
    cap (mk (1.52f, 0.98f, -0.38f), mk (1.50f, 1.12f, -0.30f), 0.0085f);
    tripod (mk (1.66f, 0, -0.10f), 0.70f, 0.36f, 0.0105f);
    cap (mk (1.66f, 0.70f, -0.10f), mk (1.62f, 0.82f, 0.04f), 0.0085f);

    // snare stand with basket
    {
        const Drum& s = drums[5];
        tripod (mk (s.pos.x, 0, s.pos.z), s.pos.y - 0.085f, 0.34f, 0.0115f);
        for (int i = 0; i < 3; ++i)
        {
            const float ang = 0.5236f + (float) i * 2.0944f;
            cap (mk (s.pos.x, s.pos.y - 0.085f, s.pos.z),
                 mk (s.pos.x + cosf (ang) * 0.15f, s.pos.y - 0.052f, s.pos.z + sinf (ang) * 0.15f), 0.006f);
        }
    }

    // rack tom mounts on the kick
    {
        const V m = mk (0.0f, 0.585f, -0.04f);
        UPD (len (p - m) - 0.028f, M_CHROME, 20, p, 0.f, 0.f);
        cap (m, drums[1].pos + mk (0.0f, -0.075f, -0.03f), 0.0085f);
        cap (m, drums[2].pos + mk (0.0f, -0.08f, -0.03f), 0.0085f);
        cap (m, mk (0.0f, 0.70f, -0.04f), 0.011f);
    }

    // low tom stand
    {
        const Drum& t = drums[3];
        tripod (mk (t.pos.x + 0.02f, 0, t.pos.z - 0.08f), 0.60f, 0.38f, 0.0115f);
        cap (mk (t.pos.x + 0.02f, 0.60f, t.pos.z - 0.08f), t.pos + mk (0.0f, -0.12f, -0.02f), 0.0095f);
    }

    // floor tom legs
    {
        const Drum& f = drums[4];
        for (int i = 0; i < 3; ++i)
        {
            const float ang = 0.70f + (float) i * 2.0944f;
            const V top = mk (f.pos.x + cosf (ang) * (f.R + 0.014f), f.pos.y + 0.02f, f.pos.z + sinf (ang) * (f.R + 0.014f));
            const V bot = mk (f.pos.x + cosf (ang) * (f.R + 0.05f), 0.012f, f.pos.z + sinf (ang) * (f.R + 0.05f));
            cap (top, bot, 0.0105f);
            UPD (len (p - bot) - 0.016f, M_BLACK, 20, p, 0.f, 0.f);
            UPD (sdBox (p - (top + mk (0.f, -0.02f, 0.f)), mk (0.017f, 0.04f, 0.017f)), M_CHROME, 20, p, 0.f, 0.f);
        }
    }

    // kick: spurs + pedal
    for (int s = -1; s <= 1; s += 2)
    {
        cap (mk ((float) s * 0.235f, 0.20f, 0.13f), mk ((float) s * 0.42f, 0.016f, 0.34f), 0.0095f);
        UPD (len (p - mk ((float) s * 0.42f, 0.016f, 0.34f)) - 0.017f, M_BLACK, 20, p, 0.f, 0.f);
    }
    UPD (sdBox (p - mk (0.02f, 0.012f, 0.56f), mk (0.065f, 0.008f, 0.15f)), M_CHROME, 20, p, 0.f, 0.f);
    {
        V q = rotX (p - mk (0.02f, 0.05f, 0.58f), 0.20f);
        UPD (sdBox (q, mk (0.055f, 0.008f, 0.11f)) - 0.004f, M_CHROME, 20, p, 0.f, 0.f);
    }
    cap (mk (0.02f, 0.06f, 0.46f), mk (0.02f, 0.20f, 0.268f), 0.0065f);
    UPD (len (p - mk (0.02f, 0.205f, 0.258f)) - 0.024f, M_BLACK, 20, p, 0.f, 0.f);

    return best;
}

// ------------------------------------------------------------------ environment
static float bm (float v, float c, float w, float soft) { return 1.0f - sstep (w - soft, w + soft, fabsf (v - c)); }

static V envMap (V d, bool sharp)
{
    const float up = d.y;
    V c = mixv (mk (0.03f, 0.02f, 0.015f), mk (0.20f, 0.12f, 0.07f), sstep (-0.3f, 0.7f, up));
    c = c + mk (0.10f, 0.12f, 0.15f) * expf (-up * up * 30.f);

    if (sharp)
    {
        c = c + mk (3.2f, 3.1f, 3.0f) * (sstep (0.45f, 0.52f, up) * bm (d.x, 0.0f, 0.50f, 0.08f) * bm (d.z, -0.05f, 0.50f, 0.08f));
        c = c + mk (2.4f, 2.3f, 2.2f) * (bm (d.x, -0.85f, 0.20f, 0.08f) * bm (up, 0.35f, 0.40f, 0.10f) * bm (d.z, 0.15f, 0.55f, 0.10f));
        c = c + mk (1.2f, 1.6f, 2.4f) * (bm (d.x, 0.85f, 0.18f, 0.08f) * bm (up, 0.30f, 0.40f, 0.10f) * bm (d.z, 0.20f, 0.50f, 0.10f));
        c = c + mk (3.0f, 2.6f, 2.0f) * (bm (d.z, -0.90f, 0.25f, 0.08f) * bm (up, 0.30f, 0.50f, 0.10f) * bm (d.x, 0.0f, 0.70f, 0.10f));
        c = c + mk (2.4f, 2.4f, 2.5f) * (bm (d.z, 0.80f, 0.30f, 0.10f) * bm (up, 0.25f, 0.35f, 0.10f) * bm (d.x, -0.15f, 0.60f, 0.10f));
    }
    else
        c = c + mk (0.20f, 0.20f, 0.21f) * sstep (-0.2f, 0.8f, up);

    return c;
}

static V background (V rd)
{
    const float t = clampf (rd.y * 2.2f + 0.35f, 0.f, 1.f);
    V c = mixv (mk (0.045f, 0.024f, 0.012f), mk (0.008f, 0.004f, 0.002f), sstep (0.0f, 1.0f, t));

    const float gx = rd.x - 0.04f, gy = rd.y - 0.05f;
    c = c + mk (0.40f, 0.16f, 0.045f) * (0.30f * expf (-(gx * gx * 9.f + gy * gy * 30.f)));

    // studio bokeh lights
    const float u = rd.x / (-rd.z + 0.001f), v = rd.y / (-rd.z + 0.001f);
    static const float B[][5] = { { -0.55f, 0.33f, 0.07f, 1.0f, 0.12f }, { -0.30f, 0.45f, 0.05f, 0.85f, 0.10f }, { 0.10f, 0.52f, 0.09f, 0.7f, 0.10f },
                                  { 0.42f, 0.38f, 0.06f, 1.0f, 0.13f },  { 0.62f, 0.25f, 0.08f, 0.8f, 0.10f },  { -0.75f, 0.12f, 0.06f, 0.9f, 0.10f },
                                  { 0.80f, 0.05f, 0.05f, 0.9f, 0.09f },  { -0.12f, 0.20f, 0.10f, 0.6f, 0.07f } };
    for (const auto& b : B)
    {
        const float dd = sqrtf ((u - b[0]) * (u - b[0]) + (v - b[1]) * (v - b[1]));
        const float a = (1.0f - sstep (b[2] * 0.85f, b[2], dd)) * b[4];
        c = c + mk (1.0f, 0.62f * b[3] + 0.04f, 0.28f * b[3] + 0.05f) * (a * 0.35f);
    }

    return c;
}

// ------------------------------------------------------------------ shading
struct Mat { V albedo; float metal, rough, f0; };

static V gNormalBias;

static V calcNormal (V p)
{
    const float e = 0.0007f;
    const V k1 = mk (1, -1, -1), k2 = mk (-1, -1, 1), k3 = mk (-1, 1, -1), k4 = mk (1, 1, 1);
    return norm (k1 * map (p + k1 * e, nullptr) + k2 * map (p + k2 * e, nullptr) + k3 * map (p + k3 * e, nullptr) + k4 * map (p + k4 * e, nullptr));
}

static bool march (V ro, V rd, float& tout, Info& I)
{
    float t = 0.05f;
    for (int i = 0; i < 260; ++i)
    {
        Info tmp;
        const float d = map (ro + rd * t, &tmp);
        if (d < 0.0004f + 0.00025f * t && tmp.mat != M_NONE)
        {
            tout = t;
            I = tmp;
            return true;
        }

        t += max (d, 0.0004f) * 0.9f;
        if (t > 16.f) break;
    }

    return false;
}

static float softShadow (V ro, V rd, float k)
{
    float res = 1.0f, t = 0.02f;
    for (int i = 0; i < 56; ++i)
    {
        const float h = map (ro + rd * t, nullptr);
        if (h < 0.0004f) return 0.0f;
        res = min (res, k * h / t);
        t += clampf (h, 0.012f, 0.22f);
        if (t > 6.f) break;
    }
    return clampf (res, 0.f, 1.f);
}

static float calcAO (V p, V n)
{
    float occ = 0, sca = 1;
    for (int i = 1; i <= 5; ++i)
    {
        const float h = 0.025f * (float) i;
        occ += (h - map (p + n * h, nullptr)) * sca;
        sca *= 0.72f;
    }
    return clampf (1.0f - 2.4f * occ, 0.f, 1.f);
}

static Mat getMat (const Info& I, V p, V& n)
{
    Mat m;
    const V q = I.q;

    switch (I.mat)
    {
        case M_FLOOR:
        {
            const float pool = 0.10f + 1.1f * expf (-((p.x - 0.05f) * (p.x - 0.05f) + (p.z + 0.05f) * (p.z + 0.05f)) / (2.f * 1.35f * 1.35f));
            const float pl = floorf (p.x / 0.26f);
            const float pr = hash21 (pl, 3.7f);
            const float grain = 0.72f + 0.28f * vnoise (p.x * 24.f + pr * 40.f, p.z * 2.2f) + 0.10f * vnoise (p.x * 90.f, p.z * 7.f);
            const float seam = 1.0f - 0.65f * (1.0f - sstep (0.0f, 0.008f, fabsf (p.x / 0.26f - pl - 0.0f)) ) ;
            const V wood = mixv (mk (0.34f, 0.16f, 0.06f), mk (0.50f, 0.26f, 0.10f), pr);
            m.albedo = wood * (pool * grain * seam);
            m.metal = 0; m.rough = 0.40f; m.f0 = 0.04f;
            break;
        }
        case M_WOOD:
        case M_WOODHOOP:
        {
            const float R = I.aux, H = I.aux2;
            const float c = atan2f (q.z, q.x) * R;
            const float g1 = vnoise (c * 70.f, q.y * 4.f) * 0.6f + vnoise (c * 230.f, q.y * 9.f) * 0.4f;
            const float stripe = 0.5f + 0.5f * sinf (c * 190.f + 7.f * vnoise (c * 25.f, q.y * 2.f));
            const float tone = 0.76f + 0.24f * mixf (g1, stripe, 0.5f);
            const float swirl = vnoise (c * 38.f + 3.f * vnoise (q.y * 6.f, c * 9.f), q.y * 5.f);
            const float dark = sstep (0.50f, 0.80f, vnoise (c * 55.f, q.y * 3.f + 2.f * swirl));   // olive-ash dark veining
            V base = mixv (mk (0.86f, 0.55f, 0.24f), mk (0.62f, 0.36f, 0.13f), 0.80f * stripe + 0.35f * swirl);
            base = mixv (base, mk (0.26f, 0.14f, 0.05f), 0.60f * dark);
            if (I.mat == M_WOODHOOP) base = base * 0.9f;
            m.albedo = base * (0.70f + 0.55f * g1);
            m.metal = 0; m.rough = 0.34f; m.f0 = 0.05f;
            break;
        }
        case M_HEAD:
        {
            const float r = sqrtf (q.x * q.x + q.z * q.z) / I.aux;
            const float ring = 0.04f * sinf (r * 120.f) * (1.f - r);
            const float sheen = 0.04f * vnoise (q.x * 90.f, q.z * 90.f);
            const float edge = sstep (0.86f, 1.0f, r);
            m.albedo = mk (0.88f, 0.87f, 0.82f) * (1.0f + ring + sheen - 0.22f * edge);
            m.metal = 0; m.rough = 0.45f; m.f0 = 0.035f;
            break;
        }
        case M_KICKHEAD:
        {
            const float R = I.aux;
            const float r = sqrtf (q.x * q.x + q.z * q.z) / R;
            const float ring = 0.04f * sinf (r * 120.f) * (1.f - r);
            const float sheen = 0.04f * vnoise (q.x * 90.f, q.z * 90.f);
            V col = mk (0.025f, 0.025f, 0.028f) * (1.0f + 2.0f * ring + 3.0f * sheen);

            // African Gospel logo (centre, slightly high)
            V rgb; float a;
            texAg.sample (0.5f + q.x / (1.05f * R), 0.5f + (q.z + 0.05f * R) / (0.74f * R), rgb, a);
            if (a > 0) col = mixv (col, rgb, a);

            // port hole ring (lower left)
            const float pr = sqrtf ((q.x + 0.52f * R) * (q.x + 0.52f * R) + (q.z - 0.52f * R) * (q.z - 0.52f * R)) / R;
            if (pr < 0.075f) col = mk (0.004f, 0.004f, 0.004f);
            else if (pr < 0.092f) col = mk (0.80f, 0.82f, 0.86f);

            m.albedo = col;
            m.metal = 0; m.rough = 0.45f; m.f0 = 0.04f;
            break;
        }
        case M_CHROME:
            m.albedo = mk (0.93f, 0.94f, 0.96f); m.metal = 1; m.rough = 0.06f; m.f0 = 0.9f;
            break;
        case M_BLACKNI:
            m.albedo = mk (0.07f, 0.07f, 0.08f); m.metal = 1; m.rough = 0.22f; m.f0 = 0.9f;
            break;
        case M_STEEL:
            m.albedo = mk (0.86f, 0.88f, 0.91f); m.metal = 1; m.rough = 0.10f; m.f0 = 0.9f;
            break;
        case M_BRASS:
        {
            const float r = sqrtf (q.x * q.x + q.z * q.z);
            const float rings = 0.5f + 0.5f * sinf (r * 1700.f + 4.f * vnoise (r * 60.f, 0.f));
            const float ang = atan2f (q.z, q.x);
            const float sheen = powf (0.5f + 0.5f * cosf (2.f * (ang - (float) I.obj * 0.9f)), 3.f);
            const float hammer = vnoise (q.x * 260.f, q.z * 260.f);
            const float lace = sstep (0.18f * I.aux, 0.24f * I.aux, r);   // bell region polished
            V base = mk (0.85f, 0.55f, 0.18f);
            m.albedo = base * (0.70f + 0.18f * rings + 0.55f * sheen * (0.5f + 0.5f * lace) + 0.10f * hammer);
            m.metal = 1; m.rough = 0.20f + 0.08f * rings; m.f0 = 0.9f;
            break;
        }
        default:
            m.albedo = mk (0.02f, 0.02f, 0.022f); m.metal = 0; m.rough = 0.6f; m.f0 = 0.04f;
            break;
    }

    (void) n;
    return m;
}

static const V kLightDir[3] = { norm (mk (-0.55f, 0.85f, 0.65f)), norm (mk (0.85f, 0.45f, 0.55f)), norm (mk (0.25f, 0.65f, -0.8f)) };
static const V kLightCol[3] = { mk (1.25f, 1.02f, 0.78f), mk (0.55f, 0.28f, 0.12f), mk (0.95f, 0.62f, 0.38f) };

static V trace (V ro, V rd, int depth)
{
    float t;
    Info I;
    if (! march (ro, rd, t, I))
        return depth == 0 ? background (rd) : envMap (rd, true);

    const V p = ro + rd * t;
    V n = calcNormal (p);
    const Mat m = getMat (I, p, n);

    const float ao = depth == 0 ? calcAO (p, n) : 1.0f;
    V diff = mixv (mk (0.05f, 0.035f, 0.025f), mk (0.20f, 0.14f, 0.10f), n.y * 0.5f + 0.5f) * ao;

    for (int i = 0; i < 3; ++i)
    {
        const float nl = max (dot (n, kLightDir[i]), 0.f);
        if (nl <= 0.f) continue;
        const float sh = (depth == 0 && i != 1) ? softShadow (p + n * 0.003f, kLightDir[i], 10.f) : 1.0f;
        diff = diff + kLightCol[i] * (nl * sh);
    }

    // Fresnel + reflection
    const float cosv = max (dot (n, rd * -1.0f), 0.f);
    const float fr = powf (1.0f - cosv, 5.0f);
    const V F0 = mixv (mk (m.f0, m.f0, m.f0), m.albedo, m.metal);
    const V F = F0 + (mk (1, 1, 1) - F0) * fr;

    V rc = mk (0, 0, 0);
    if (max (F.x, max (F.y, F.z)) > 0.025f)
    {
        const V rdir = refl (rd, n);
        rc = depth < 2 ? trace (p + n * 0.003f, rdir, depth + 1) : envMap (rdir, true);
        const float blur = clampf ((m.rough - 0.12f) * 2.2f, 0.f, 0.75f);
        if (blur > 0) rc = mixv (rc, envMap (rdir, false), blur);
    }

    V col = m.albedo * diff * (1.0f - m.metal);
    col = col * (mk (1, 1, 1) - F) + rc * F * (I.mat == M_FLOOR ? 0.35f : 0.8f);

    if (I.mat == M_FLOOR)
    {
        const float fog = 1.0f - expf (-max (t - 2.8f, 0.f) * 0.33f);
        col = mixv (col, background (rd), clampf (fog, 0.f, 1.f));
    }

    return col;
}

// ------------------------------------------------------------------ camera + layout
static V camPos, camF, camR, camU;
static float tanY, aspectW;

static void project (V w, float W, float H, float& px, float& py)
{
    const V d = w - camPos;
    const float z = dot (d, camF);
    const float x = dot (d, camR) / (z * tanY * aspectW);
    const float y = dot (d, camU) / (z * tanY);
    px = (x * 0.5f + 0.5f) * W;
    py = (0.5f - y * 0.5f) * H;
}

int main (int argc, char** argv)
{
    const int W = argc > 1 ? atoi (argv[1]) : 1650;
    const int spp = argc > 2 ? atoi (argv[2]) : 4;
    const char* outRaw = argc > 3 ? argv[3] : "kit.raw";
    const char* outLayout = argc > 4 ? argv[4] : "layout.inc";
    const int H = (int) lroundf ((float) W * 404.0f / 660.0f);

    texAg.load ("decal_ag.bin");

    // drums: pos, yaw, tilt, R, H, obj, kind(0 tom,1 snare,2 kick), lugs
    drums[0] = { mk (0.00f, 0.30f, 0.00f), 0.0f, 1.5708f, 0.280f, 0.460f, 1, 2, 10 };   // kick
    drums[1] = { mk (-0.185f, 0.865f, -0.06f), 0.10f, 0.30f, 0.152f, 0.200f, 2, 0, 6 }; // high tom
    drums[2] = { mk (0.170f, 0.870f, -0.06f), -0.10f, 0.30f, 0.165f, 0.230f, 3, 0, 6 }; // mid tom
    drums[3] = { mk (0.510f, 0.790f, -0.10f), -0.18f, 0.26f, 0.178f, 0.300f, 4, 0, 8 }; // low tom
    drums[4] = { mk (0.730f, 0.420f, 0.14f), -0.20f, 0.05f, 0.205f, 0.410f, 5, 0, 8 }; // floor tom
    drums[5] = { mk (-0.500f, 0.620f, 0.10f), 0.15f, 0.10f, 0.180f, 0.140f, 6, 1, 8 };  // snare

    // cymbals: pos, yaw, tilt, tiltz, R, bell, bellH, sag, obj
    cyms[0] = { mk (-0.62f, 1.36f, -0.40f), 0.3f, 0.20f, 0.22f, 0.230f, 0.048f, 0.030f, 0.030f, 10 };   // crash 1
    cyms[1] = { mk (0.16f, 1.34f, -0.46f), -0.1f, 0.12f, -0.06f, 0.275f, 0.085f, 0.052f, 0.026f, 11 };  // splash / crash 2 (centre, larger)
    cyms[2] = { mk (1.05f, 1.12f, -0.22f), 0.2f, 0.14f, -0.12f, 0.265f, 0.050f, 0.034f, 0.020f, 12 };   // ride
    cyms[3] = { mk (kFootHH.x, 0.915f, kFootHH.z), 0.0f, 0.05f, 0.0f, 0.185f, 0.040f, 0.022f, 0.012f, 13 };           // hi-hat top
    cyms[4] = { mk (kFootHH.x, 0.893f, kFootHH.z), 0.0f, 0.05f, 3.14159f, 0.180f, 0.040f, 0.022f, 0.010f, 14 };       // hi-hat bottom

    cyms[5] = { mk (-0.30f, 1.55f, -0.68f), 0.1f, 0.16f, 0.10f, 0.250f, 0.050f, 0.032f, 0.030f, 15 };   // crash 2
    cyms[6] = { mk (0.62f, 1.52f, -0.64f), -0.2f, 0.16f, -0.10f, 0.250f, 0.050f, 0.032f, 0.030f, 16 };   // crash 3
    cyms[7] = { mk (-1.13f, 1.11f, -0.22f), 0.3f, 0.10f, 0.22f, 0.170f, 0.062f, 0.040f, 0.020f, 17 };    // splash 2
    cyms[8] = { mk (1.50f, 1.14f, -0.30f), -0.3f, 0.20f, -0.22f, 0.235f, 0.060f, 0.040f, 0.015f, 18 };   // china
    cyms[9] = { mk (1.62f, 0.84f, 0.04f), -0.2f, 0.14f, -0.20f, 0.215f, 0.050f, 0.030f, 0.020f, 19, 1 }; // FX with holes

    // camera
    camPos = mk (0.14f, 1.12f, 4.30f);
    const V target = mk (0.14f, 0.69f, 0.0f);
    camF = norm (target - camPos);
    camR = norm (cross (camF, mk (0, 1, 0)));
    camU = cross (camR, camF);
    tanY = tanf (0.5f * 28.5f * 3.14159265f / 180.0f);
    aspectW = (float) W / (float) H;

    // ---- render
    std::vector<float> img ((size_t) W * H * 3);
    const int grid = max (1, (int) lroundf (sqrtf ((float) spp)));

#pragma omp parallel for schedule(dynamic, 4)
    for (int y = 0; y < H; ++y)
    {
        for (int x = 0; x < W; ++x)
        {
            V acc = mk (0, 0, 0);
            int n = 0;

            for (int sy = 0; sy < grid; ++sy)
                for (int sx = 0; sx < grid; ++sx)
                {
                    const float jx = (sx + hash21 ((float) x * 1.7f + sx, (float) y * 2.3f + sy)) / (float) grid;
                    const float jy = (sy + hash21 ((float) y * 3.1f + sy, (float) x * 1.3f + sx)) / (float) grid;
                    const float ndcx = ((x + jx) / (float) W) * 2.f - 1.f;
                    const float ndcy = 1.f - ((y + jy) / (float) H) * 2.f;
                    const V rd = norm (camF + camR * (ndcx * tanY * aspectW) + camU * (ndcy * tanY));
                    V c = trace (camPos, rd, 0);
                    // firefly clamp
                    c.x = min (c.x, 12.f); c.y = min (c.y, 12.f); c.z = min (c.z, 12.f);
                    acc = acc + c;
                    ++n;
                }

            acc = acc * (1.0f / (float) n);
            float* o = &img[((size_t) y * W + x) * 3];
            o[0] = acc.x; o[1] = acc.y; o[2] = acc.z;
        }

        if (y % 40 == 0)
        {
#pragma omp critical
            { fprintf (stderr, "row %d/%d\n", y, H); }
        }
    }

    FILE* f = fopen (outRaw, "wb");
    fwrite (&W, 4, 1, f); fwrite (&H, 4, 1, f);
    fwrite (img.data(), 4, img.size(), f);
    fclose (f);

    // ---- layout: project each playable part into the 660x404 design space used by DrumKitView
    const float k = 660.0f / (float) W;
    FILE* L = fopen (outLayout, "w");

    auto drumRegion = [&] (const Drum& D, const char* type, const char* name, int pad, int shift, int alt)
    {
        const V up = rotY (rotX (mk (0, 1, 0), D.tilt), D.yaw);
        const V ex = rotY (rotX (mk (1, 0, 0), D.tilt), D.yaw);
        const V ez = rotY (rotX (mk (0, 0, 1), D.tilt), D.yaw);
        const V c = D.pos + up * (D.H * 0.5f + 0.004f);
        float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
        for (int i = 0; i < 48; ++i)
        {
            const float a = (float) i / 48.f * 6.2831853f;
            float px, py;
            project (c + ex * (cosf (a) * D.R) + ez * (sinf (a) * D.R), (float) W, (float) H, px, py);
            x0 = min (x0, px); x1 = max (x1, px); y0 = min (y0, py); y1 = max (y1, py);
        }
        float bx, by, cx, cy;
        project (D.pos - up * (D.H * 0.5f), (float) W, (float) H, bx, by);
        project (c, (float) W, (float) H, cx, cy);
        float bodyH = D.kind == 2 ? 0.f : max (0.f, (by - cy) * k);
        const float rx = (x1 - x0) * 0.5f * k, ry = (y1 - y0) * 0.5f * k;
        fprintf (L, "    parts.push_back ({ Type::%s, \"%s\", %.1ff, %.1ff, %.1ff, %.1ff, %.1ff, 0.0f, %d, %d, %d });\n",
                 type, name, (x0 + x1) * 0.5f * k, (y0 + y1) * 0.5f * k, rx, D.kind == 2 ? rx : ry, bodyH, pad, shift, alt);
    };

    auto cymRegion = [&] (const Cym& C, const char* type, const char* name, int pad, int shift)
    {
        float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
        for (int i = 0; i < 48; ++i)
        {
            const float a = (float) i / 48.f * 6.2831853f;
            const V l = mk (cosf (a) * C.R, 0.f, sinf (a) * C.R);
            const V w = C.pos + rotY (rotX (rotZ (l, C.tiltz), C.tilt), C.yaw);
            float px, py;
            project (w, (float) W, (float) H, px, py);
            x0 = min (x0, px); x1 = max (x1, px); y0 = min (y0, py); y1 = max (y1, py);
        }
        fprintf (L, "    parts.push_back ({ Type::%s, \"%s\", %.1ff, %.1ff, %.1ff, %.1ff, 0.0f, 0.0f, %d, %d, -1 });\n",
                 type, name, (x0 + x1) * 0.5f * k, (y0 + y1) * 0.5f * k, (x1 - x0) * 0.5f * k, max ((y1 - y0) * 0.5f * k, 6.0f), pad, shift);
    };

    cymRegion (cyms[0], "Cymbal", "CRASH 1", 12, -1);
    cymRegion (cyms[3], "HiHat", "HI-HAT", 9, 11);
    cymRegion (cyms[1], "Cymbal", "SPLASH", 13, -1);
    cymRegion (cyms[2], "Cymbal", "RIDE", 14, -1);
    cymRegion (cyms[5], "Cymbal", "CRASH 2", 16, -1);
    cymRegion (cyms[6], "Cymbal", "CRASH 3", 17, -1);
    cymRegion (cyms[7], "Cymbal", "SPLASH 2", 18, -1);
    cymRegion (cyms[8], "Cymbal", "CHINA", 19, -1);
    cymRegion (cyms[9], "Cymbal", "FX CYMBAL", 20, -1);
    drumRegion (drums[0], "Kick", "KICK", 0, -1, 1);
    drumRegion (drums[1], "Tom", "HIGH TOM", 8, -1, -1);
    drumRegion (drums[2], "Tom", "MID TOM", 7, -1, -1);
    drumRegion (drums[3], "Tom", "LOW TOM", 6, -1, -1);
    drumRegion (drums[4], "Tom", "FLOOR TOM", 5, -1, -1);
    drumRegion (drums[5], "Snare", "SNARE", 2, 4, -1);
    {
        float px, py, qx, qy;
        project (mk (kFootHH.x, 0.04f, kFootHH.z + 0.20f), (float) W, (float) H, px, py);
        project (mk (kFootHH.x + 0.06f, 0.04f, kFootHH.z + 0.28f), (float) W, (float) H, qx, qy);
        fprintf (L, "    parts.push_back ({ Type::Pedal, \"HH PEDAL\", %.1ff, %.1ff, %.1ff, %.1ff, 0.0f, 0.0f, 10, -1, -1 });\n",
                 px * k, py * k, max (18.0f, fabsf (qx - px) * k * 1.2f), max (9.0f, fabsf (qy - py) * k * 1.0f));
    }
    fclose (L);
    fprintf (stderr, "done %dx%d\n", W, H);
    return 0;
}
