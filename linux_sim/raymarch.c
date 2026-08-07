#ifdef _WIN32
#include <windows.h>
#endif
/**
 * raymarch.c — v5
 * 【v5】pthread 行级并行: N 核同时光追, 帧率 ≈ 单核 × N
 * 场景同 v4: 解析地面/球 + 步进圆环 + 软阴影 + 雾
 */
#include "raymarch.h"
#include <math.h>
#include <pthread.h>
#include <unistd.h>

typedef struct { float x, y, z; } v3;
static v3 V(float x, float y, float z) { v3 r = {x,y,z}; return r; }
static v3 vadd(v3 a, v3 b) { return V(a.x+b.x, a.y+b.y, a.z+b.z); }
static v3 vsub(v3 a, v3 b) { return V(a.x-b.x, a.y-b.y, a.z-b.z); }
static v3 vmul(v3 a, float t) { return V(a.x*t, a.y*t, a.z*t); }
static float vdot(v3 a, v3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static v3 vcross(v3 a, v3 b) { return V(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x); }
static float vlen(v3 a) { return sqrtf(vdot(a,a)); }
static v3 vnorm(v3 a) { return vmul(a, 1.0f/(vlen(a)+1e-9f)); }

static float torus_sdf(v3 p, float t)
{
    float ca = cosf(t*0.6f), sa = sinf(t*0.6f);
    float x =  p.x*ca - p.z*sa;
    float z =  p.x*sa + p.z*ca;
    float y2 = p.y*0.62f - z*0.78f;
    float z2 = p.y*0.78f + z*0.62f;
    float q = sqrtf(x*x + y2*y2) - 1.2f;
    return sqrtf(q*q + z2*z2) - 0.35f;
}

static v3 torus_normal(v3 p, float t)
{
    const float e = 0.002f;
    float d = torus_sdf(p, t);
    return vnorm(V(torus_sdf(vadd(p,V(e,0,0)),t)-d,
                   torus_sdf(vadd(p,V(0,e,0)),t)-d,
                   torus_sdf(vadd(p,V(0,0,e)),t)-d));
}

static float map_all(v3 p, float t, v3 sc, float sr)
{
    float d = p.y + 1.0f;
    float ds = vlen(vsub(p, sc)) - sr;
    if (ds < d) d = ds;
    float dt = torus_sdf(p, t);
    if (dt < d) d = dt;
    return d;
}

static float shadow(v3 ro, v3 rd, float t, v3 sc, float sr)
{
    float tt = 0.03f;
    for (int i = 0; i < 24; i++) {
        float h = map_all(vadd(ro, vmul(rd, tt)), t, sc, sr);
        if (h < 0.002f) return 0.3f;
        tt += h;
        if (tt > 8.0f) break;
    }
    return 1.0f;
}

static uint16_t to565(float r, float g, float b)
{
    if (r<0) r=0; if (r>1) r=1;
    if (g<0) g=0; if (g>1) g=1;
    if (b<0) b=0; if (b>1) b=1;
    return (uint16_t)((((int)(r*31.99f))<<11) | (((int)(g*63.99f))<<5) | ((int)(b*31.99f)));
}

/* ---------- 并行任务 ---------- */
typedef struct {
    uint16_t *px;
    int w, h, y0, y1;
    float time; int max_steps;
    v3 ro, cu, cv, cw, ldir, sc; float sr;
} job_t;

static void shade_rows(job_t *J)
{
    int w = J->w, h = J->h;
    for (int j = J->y0; j < J->y1; j++) {
        float v = (h - 2.0f*j) / (float)h;
        for (int i = 0; i < w; i++) {
            float u = (2.0f*i - w) / (float)h;
            v3 rd = vnorm(vadd(vadd(vmul(J->cu,u), vmul(J->cv,v)), vmul(J->cw,1.6f)));

            float t_hit = 1e9f; int mat = 0;

            if (rd.y < -1e-4f) {
                float tp = -(J->ro.y + 1.0f) / rd.y;
                if (tp > 0.0f && tp < t_hit) { t_hit = tp; mat = 1; }
            }
            {
                v3 oc = vsub(J->ro, J->sc);
                float bq = vdot(oc, rd);
                float cq = vdot(oc, oc) - J->sr*J->sr;
                float disc = bq*bq - cq;
                if (disc > 0.0f) {
                    float ts = -bq - sqrtf(disc);
                    if (ts > 0.01f && ts < t_hit) { t_hit = ts; mat = 2; }
                }
            }
            {
                float tt = 0.02f;
                for (int s = 0; s < J->max_steps; s++) {
                    float d = torus_sdf(vadd(J->ro, vmul(rd, tt)), J->time);
                    if (d < 0.001f) { if (tt < t_hit) { t_hit = tt; mat = 3; } break; }
                    tt += d;
                    if (tt > t_hit || tt > 12.0f) break;
                }
            }

            float r, g, b;
            if (!mat) {
                float sk = 0.5f + 0.5f*rd.y; if (sk<0) sk=0;
                r = 0.15f + 0.35f*sk; g = 0.25f + 0.45f*sk; b = 0.45f + 0.5f*sk;
                float sd = vdot(rd, J->ldir); if (sd<0) sd=0;
                sd = sd*sd*sd*sd;
                r += 0.6f*sd; g += 0.45f*sd; b += 0.2f*sd;
            } else {
                v3 pos = vadd(J->ro, vmul(rd, t_hit));
                v3 n, mcol;
                if (mat == 1) {
                    n = V(0,1,0);
                    int cx = (int)floorf(pos.x*0.8f), cz = (int)floorf(pos.z*0.8f);
                    if ((cx + cz) & 1) mcol = V(0.9f, 0.9f, 0.95f);
                    else               mcol = V(0.1f, 0.15f, 0.4f);
                } else if (mat == 2) {
                    n = vnorm(vsub(pos, J->sc));
                    mcol = V(1.0f, 0.45f, 0.1f);
                } else {
                    n = torus_normal(pos, J->time);
                    mcol = V(0.9f, 0.2f, 0.7f);
                }
                float dif = vdot(n, J->ldir); if (dif<0) dif=0;
                float sh = shadow(vadd(pos, vmul(n,0.02f)), J->ldir, J->time, J->sc, J->sr);
                float spe = vdot(n, vnorm(vsub(J->ldir, rd))); if (spe<0) spe=0;
                spe = powf(spe, 24.0f) * sh;
                float li = 0.25f + 1.4f*dif*sh;
                r = mcol.x*li + spe;
                g = mcol.y*li + spe;
                b = mcol.z*li + spe;
                float fo = expf(-t_hit*0.06f);
                r = r*fo + 0.2f*(1-fo);
                g = g*fo + 0.3f*(1-fo);
                b = b*fo + 0.5f*(1-fo);
            }
            float dt2 = ((float)(((i*13 + j*7) & 3)) - 1.5f) / 512.0f;
            J->px[j*w + i] = to565(r+dt2, g+dt2, b+dt2);
        }
    }
}

static void *worker(void *a)
{
    shade_rows((job_t *)a);
    return NULL;
}

void raymarch_render(uint16_t *px, int w, int h, float time, int max_steps)
{
    if (max_steps < 8) max_steps = 8;
    float ang = time * 0.35f;

    job_t base;
    base.px = px; base.w = w; base.h = h;
    base.time = time; base.max_steps = max_steps;
    base.ro = V(3.0f*cosf(ang), 1.2f + 0.4f*sinf(time*0.23f), 3.0f*sinf(ang));
    v3 ta = V(0.0f, -0.1f, 0.0f);
    base.cw = vnorm(vsub(ta, base.ro));
    base.cu = vnorm(vcross(base.cw, V(0,1,0)));
    base.cv = vcross(base.cu, base.cw);
    base.ldir = vnorm(V(0.6f, 0.7f, -0.35f));
    base.sc = V(sinf(time*0.9f)*1.6f, 0.2f + 0.4f*sinf(time*1.3f), cosf(time*0.7f)*0.9f);
    base.sr = 0.8f;

    /* 【v5 核心】按行切分, N 核并行 */
    int n;
#ifdef _WIN32
    SYSTEM_INFO sysinfo; GetSystemInfo(&sysinfo); n = sysinfo.dwNumberOfProcessors;
#else
    n = (int)sysconf(_SC_NPROCESSORS_ONLN);
#endif
    if (n < 1) n = 1;
    if (n > 8) n = 8;
    if (n > h) n = h;

    if (n <= 1) {
        base.y0 = 0; base.y1 = h;
        shade_rows(&base);
        return;
    }
    pthread_t th[8];
    job_t jobs[8];
    for (int t = 0; t < n; t++) {
        jobs[t] = base;
        jobs[t].y0 = h * t / n;
        jobs[t].y1 = h * (t + 1) / n;
        pthread_create(&th[t], NULL, worker, &jobs[t]);
    }
    for (int t = 0; t < n; t++) pthread_join(th[t], NULL);
}