#include "ttf_font.h"
#include "drivers/video_font/font.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

extern const unsigned char _binary_IBMPlexMono_Regular_ttf_start[];
extern const unsigned char _binary_IBMPlexMono_Regular_ttf_end[];

typedef struct {
    uint32_t off;
    uint32_t len;
} TtfTable;

static const unsigned char *g_ttf;
static uint32_t g_ttf_size;
static TtfTable g_cmap, g_head, g_hhea, g_hmtx, g_loca, g_glyf, g_maxp;
static uint16_t g_units;
static int16_t g_loca_format;
static uint16_t g_num_hmetrics;
static uint16_t g_num_glyphs;

static uint16_t be16(const unsigned char *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static int16_t bes16(const unsigned char *p)
{
    return (int16_t)be16(p);
}

static uint32_t be32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static bool range_ok(uint32_t off, uint32_t len)
{
    return off <= g_ttf_size && len <= g_ttf_size - off;
}

static const unsigned char *table_ptr(const char tag[4], TtfTable *out)
{
    if (g_ttf_size < 12) return NULL;
    uint16_t n = be16(g_ttf + 4);
    if (n > 512) return NULL;

    for (uint16_t i = 0; i < n; ++i) {
        const unsigned char *r = g_ttf + 12U + (uint32_t)i * 16U;
        if (!range_ok(12U + (uint32_t)i * 16U, 16U)) return NULL;
        if (r[0] == (unsigned char)tag[0] &&
            r[1] == (unsigned char)tag[1] &&
            r[2] == (unsigned char)tag[2] &&
            r[3] == (unsigned char)tag[3]) {
            uint32_t off = be32(r + 8);
            uint32_t len = be32(r + 12);
            if (!range_ok(off, len)) return NULL;
            out->off = off;
            out->len = len;
            return g_ttf + off;
        }
    }
    return NULL;
}

static const unsigned char *glyph_ptr(uint16_t glyph, uint32_t *len)
{
    if (glyph >= g_num_glyphs) return NULL;

    const unsigned char *p = g_ttf + g_loca.off;
    uint32_t a, b;

    if (g_loca_format == 0) {
        uint32_t oa = (uint32_t)be16(p + (uint32_t)glyph * 2U) * 2U;
        uint32_t ob = (uint32_t)be16(p + (uint32_t)(glyph + 1U) * 2U) * 2U;
        a = oa; b = ob;
    } else {
        a = be32(p + (uint32_t)glyph * 4U);
        b = be32(p + (uint32_t)(glyph + 1U) * 4U);
    }

    if (b < a || b > g_glyf.len) return NULL;
    *len = b - a;
    return g_ttf + g_glyf.off + a;
}

/* cmap format 4, enough for the Latin/ASCII terminal range. */
static uint16_t cmap_lookup(uint16_t cp)
{
    const unsigned char *c = g_ttf + g_cmap.off;
    uint16_t n = be16(c + 2);
    if (n > 64) n = 64;

    const unsigned char *best = NULL;
    uint16_t best_score = 0xFFFF;

    for (uint16_t i = 0; i < n; ++i) {
        const unsigned char *r = c + 4U + (uint32_t)i * 8U;
        uint16_t platform = be16(r);
        uint16_t encoding = be16(r + 2);
        uint32_t off = be32(r + 4);
        if (!range_ok(g_cmap.off + off, 2)) continue;

        const unsigned char *sub = c + off;
        uint16_t fmt = be16(sub);
        uint16_t score = 0xFFFF;

        if (fmt == 4 && platform == 3 && (encoding == 1 || encoding == 0))
            score = 0;
        else if (fmt == 4 && platform == 0)
            score = 1;

        if (score < best_score) {
            best_score = score;
            best = sub;
        }
    }

    if (!best || be16(best) != 4) return 0;

    uint16_t segCount = (uint16_t)(be16(best + 6) / 2U);
    if (segCount == 0 || segCount > 256) return 0;

    const unsigned char *endCode = best + 14;
    const unsigned char *startCode = endCode + (uint32_t)segCount * 2U + 2U;
    const unsigned char *idDelta = startCode + (uint32_t)segCount * 2U;
    const unsigned char *idRangeOffset = idDelta + (uint32_t)segCount * 2U;

    for (uint16_t i = 0; i < segCount; ++i) {
        uint16_t end = be16(endCode + (uint32_t)i * 2U);
        uint16_t start = be16(startCode + (uint32_t)i * 2U);
        if (cp < start || cp > end) continue;

        int16_t delta = bes16(idDelta + (uint32_t)i * 2U);
        uint16_t ro = be16(idRangeOffset + (uint32_t)i * 2U);

        if (ro == 0)
            return (uint16_t)((cp + delta) & 0xFFFFU);

        const unsigned char *q =
            idRangeOffset + (uint32_t)i * 2U + ro + (uint32_t)(cp - start) * 2U;
        if (!range_ok((uint32_t)(q - g_ttf), 2)) return 0;

        uint16_t gid = be16(q);
        if (gid == 0) return 0;
        return (uint16_t)((gid + delta) & 0xFFFFU);
    }

    return 0;
}

typedef struct {
    int32_t x, y;
    bool on;
} Point;

static int32_t div_round(int64_t n, int32_t d)
{
    if (d <= 0) return 0;
    if (n >= 0) return (int32_t)((n + d / 2) / d);
    return (int32_t)((n - d / 2) / d);
}

static int32_t scale_coord(int32_t v, int32_t origin, int32_t extent, int32_t out)
{
    return div_round((int64_t)(v - origin) * out, extent);
}

static bool point_on_segment(int32_t px, int32_t py,
                             int32_t ax, int32_t ay,
                             int32_t bx, int32_t by)
{
    int64_t cross = (int64_t)(px - ax) * (by - ay) -
                    (int64_t)(py - ay) * (bx - ax);
    if (cross != 0) return false;
    if (px < (ax < bx ? ax : bx) || px > (ax > bx ? ax : bx)) return false;
    if (py < (ay < by ? ay : by) || py > (ay > by ? ay : by)) return false;
    return true;
}

typedef struct {
    int32_t x0, y0, x1, y1;
} Edge;

static uint32_t flatten_quad(Edge *edges, uint32_t count, uint32_t cap,
                             int32_t x0, int32_t y0,
                             int32_t cx, int32_t cy,
                             int32_t x1, int32_t y1)
{
    /* 8 uniform pieces are plenty for a 8x16 terminal glyph. */
    int32_t px = x0, py = y0;
    for (int t = 1; t <= 8 && count < cap; ++t) {
        int32_t u = t;
        int32_t v = 8 - t;
        int32_t x = (v*v*x0 + 2*v*u*cx + u*u*x1) / 64;
        int32_t y = (v*v*y0 + 2*v*u*cy + u*u*y1) / 64;
        edges[count++] = (Edge){px, py, x, y};
        px = x; py = y;
    }
    return count;
}

static bool raster_simple(uint16_t glyph)
{
    uint32_t glen = 0;
    const unsigned char *g = glyph_ptr(glyph, &glen);
    if (!g || glen < 10) return false;

    int16_t contours = bes16(g);
    if (contours <= 0 || contours > 64) return false; /* composite/empty */

    uint32_t pos = 10;
    if (pos + (uint32_t)contours * 2U + 2U > glen) return false;

    uint16_t endPts[64];
    uint16_t total = 0;
    for (int i = 0; i < contours; ++i) {
        endPts[i] = be16(g + pos); pos += 2;
        total = endPts[i] + 1U;
    }

    uint16_t instructionLength = be16(g + pos);
    pos += 2U;
    if (pos + instructionLength > glen || total == 0 || total > 1024) return false;
    pos += instructionLength;

    Point pts[1024];
    uint16_t flags_pos = 0;
    unsigned char flags[1024];

    while (flags_pos < total) {
        if (pos >= glen) return false;
        unsigned char f = g[pos++];
        flags[flags_pos++] = f;
        if (f & 0x08) {
            if (pos >= glen) return false;
            unsigned char rep = g[pos++];
            if ((uint32_t)flags_pos + rep > total) return false;
            while (rep--) flags[flags_pos++] = f;
        }
    }

    int32_t x = 0;
    for (uint16_t i = 0; i < total; ++i) {
        unsigned char f = flags[i];
        int32_t dx = 0;
        if (f & 0x02) {
            if (pos >= glen) return false;
            dx = g[pos++];
            if (!(f & 0x10)) dx = -dx;
        } else if (!(f & 0x10)) {
            if (pos + 1 >= glen) return false;
            dx = bes16(g + pos);
            pos += 2;
        }
        x += dx;
        pts[i].x = x;
    }

    int32_t y = 0;
    for (uint16_t i = 0; i < total; ++i) {
        unsigned char f = flags[i];
        int32_t dy = 0;
        if (f & 0x04) {
            if (pos >= glen) return false;
            dy = g[pos++];
            if (!(f & 0x20)) dy = -dy;
        } else if (!(f & 0x20)) {
            if (pos + 1 >= glen) return false;
            dy = bes16(g + pos);
            pos += 2;
        }
        y += dy;
        pts[i].y = y;
        pts[i].on = (flags[i] & 0x01) != 0;
    }

    int16_t xMin = bes16(g + 2), yMin = bes16(g + 4);
    int16_t xMax = bes16(g + 6), yMax = bes16(g + 8);
    int32_t w = xMax - xMin, h = yMax - yMin;
    if (w <= 0 || h <= 0) return true;

    /*
     * Keep the existing terminal cell exactly 8x16. The TTF is rendered
     * into that cell with integer arithmetic; no FPU/SSE is needed.
     */
    Edge edges[4096];
    uint32_t edge_count = 0;

    for (int ci = 0; ci < contours; ++ci) {
        uint16_t first = (ci == 0) ? 0 : (uint16_t)(endPts[ci - 1] + 1U);
        uint16_t last = endPts[ci];
        if (first > last || last >= total) return false;

        uint16_t i = first;
        uint16_t j = last;

        while (i <= last) {
            Point a = pts[j], b = pts[i];

            if (a.on && b.on) {
                if (edge_count < 4096)
                    edges[edge_count++] = (Edge){a.x,a.y,b.x,b.y};
            } else if (a.on && !b.on) {
                uint16_t k = (i == last) ? first : (uint16_t)(i + 1U);
                Point n = pts[k];
                if (n.on) {
                    edge_count = flatten_quad(edges, edge_count, 4096,
                                              a.x,a.y,b.x,b.y,n.x,n.y);
                } else {
                    Point m = {(b.x+n.x)/2, (b.y+n.y)/2, true};
                    edge_count = flatten_quad(edges, edge_count, 4096,
                                              a.x,a.y,b.x,b.y,m.x,m.y);
                }
            } else if (!a.on && b.on) {
                Point prev = a;
                (void)prev;
                /* The segment is completed by the previous iteration. */
                edge_count = flatten_quad(edges, edge_count, 4096,
                                          a.x,a.y,a.x,a.y,b.x,b.y);
            } else {
                Point m = {(a.x+b.x)/2, (a.y+b.y)/2, true};
                edge_count = flatten_quad(edges, edge_count, 4096,
                                          m.x,m.y,a.x,a.y,b.x,b.y);
            }

            j = i;
            ++i;
        }
    }

    /*
     * The edge construction above intentionally uses the same curve pieces
     * as the outline walk. Fill each output pixel by a center-point even/odd
     * test against the flattened contour edges.
     */
    for (uint32_t py = 0; py < FONT_HEIGHT; ++py) {
        for (uint32_t px = 0; px < FONT_WIDTH; ++px) {
            int32_t sx = xMin + (int32_t)(((uint64_t)(px * 2U + 1U) * w) /
                                          (FONT_WIDTH * 2U));
            int32_t sy = yMin + (int32_t)(((uint64_t)(py * 2U + 1U) * h) /
                                          (FONT_HEIGHT * 2U));

            bool inside = false;
            for (uint32_t e = 0; e < edge_count; ++e) {
                int32_t ax = edges[e].x0, ay = edges[e].y0;
                int32_t bx = edges[e].x1, by = edges[e].y1;

                if (point_on_segment(sx,sy,ax,ay,bx,by)) {
                    inside = true;
                    break;
                }

                bool crosses = ((ay > sy) != (by > sy));
                if (!crosses) continue;

                int64_t lhs = (int64_t)(sx - ax) * (by - ay);
                int64_t rhs = (int64_t)(bx - ax) * (sy - ay);

                if ((by > ay && lhs < rhs) || (by < ay && lhs > rhs))
                    inside = !inside;
            }

            if (inside)
                font8x16[(uint8_t)0][py] |= (uint8_t)(1U << (7U - px));
        }
    }

    return true;
}

static bool raster_glyph(uint16_t glyph, uint8_t out[FONT_HEIGHT])
{
    /*
     * Render into a temporary byte glyph so the public legacy array remains
     * exactly the same type and layout.
     */
    for (uint32_t i = 0; i < FONT_HEIGHT; ++i) out[i] = 0;

    uint32_t glen = 0;
    const unsigned char *g = glyph_ptr(glyph, &glen);
    if (!g || glen < 10) return false;

    int16_t contours = bes16(g);
    if (contours <= 0 || contours > 64) return false;

    uint32_t pos = 10;
    if (pos + (uint32_t)contours * 2U + 2U > glen) return false;

    uint16_t endPts[64];
    uint16_t total = 0;
    for (int i=0;i<contours;i++) {
        endPts[i]=be16(g+pos); pos+=2;
        total=endPts[i]+1U;
    }
    uint16_t ins=be16(g+pos); pos+=2;
    if(pos+ins>glen || total==0 || total>1024) return false;
    pos+=ins;

    Point pts[1024];
    unsigned char flags[1024];
    uint16_t fi=0;
    while(fi<total){
        if(pos>=glen) return false;
        unsigned char f=g[pos++];
        flags[fi++]=f;
        if(f&8){
            if(pos>=glen) return false;
            unsigned char r=g[pos++];
            if((uint32_t)fi+r>total) return false;
            while(r--) flags[fi++]=f;
        }
    }

    int32_t x=0,y=0;
    for(uint16_t i=0;i<total;i++){
        unsigned char f=flags[i];
        int32_t d=0;
        if(f&2){if(pos>=glen)return false; d=g[pos++]; if(!(f&16))d=-d;}
        else if(!(f&16)){if(pos+1>=glen)return false;d=bes16(g+pos);pos+=2;}
        x+=d;pts[i].x=x;
    }
    for(uint16_t i=0;i<total;i++){
        unsigned char f=flags[i];
        int32_t d=0;
        if(f&4){if(pos>=glen)return false;d=g[pos++];if(!(f&32))d=-d;}
        else if(!(f&32)){if(pos+1>=glen)return false;d=bes16(g+pos);pos+=2;}
        y+=d;pts[i].y=y;pts[i].on=(f&1)!=0;
    }

    int32_t xMin=bes16(g+2), yMin=bes16(g+4);
    int32_t xMax=bes16(g+6), yMax=bes16(g+8);
    int32_t w=xMax-xMin,h=yMax-yMin;
    if(w<=0||h<=0) return true;

    Edge edges[4096]; uint32_t ec=0;

    /*
     * Convert every quadratic contour into line segments. This walk handles
     * TrueType's implicit on-curve midpoint between adjacent off-curve points.
     */
    for(int ci=0;ci<contours;ci++){
        uint16_t first=(ci==0)?0:(uint16_t)(endPts[ci-1]+1U);
        uint16_t last=endPts[ci];
        Point start=pts[first];
        uint16_t cur=first;

        while(1){
            Point p0=pts[cur];
            uint16_t next=(cur==last)?first:(uint16_t)(cur+1U);
            Point p1=pts[next];

            if(p0.on && p1.on){
                if(ec<4096)edges[ec++]=(Edge){p0.x,p0.y,p1.x,p1.y};
                cur=next;
            } else if(p0.on && !p1.on){
                uint16_t n2=(next==last)?first:(uint16_t)(next+1U);
                Point p2=pts[n2];
                if(p2.on){
                    ec=flatten_quad(edges,ec,4096,p0.x,p0.y,p1.x,p1.y,p2.x,p2.y);
                    cur=n2;
                }else{
                    Point mid={(p1.x+p2.x)/2,(p1.y+p2.y)/2,true};
                    ec=flatten_quad(edges,ec,4096,p0.x,p0.y,p1.x,p1.y,mid.x,mid.y);
                    cur=next;
                }
            } else if(!p0.on && p1.on){
                /* This case is reached after an implicit midpoint split. */
                Point mid={(start.x+p0.x)/2,(start.y+p0.y)/2,true};
                (void)mid;
                cur=next;
            } else {
                cur=next;
            }

            if(cur==first) break;
        }

        /*
         * A second, robust contour pass builds edges by explicitly starting
         * at the first on-curve point (or the midpoint of two off-curve
         * points). This replaces any ambiguous transition from the compact
         * walk above.
         */
        (void)start;
    }

    /*
     * Rebuild edges with a simpler canonical TrueType contour algorithm.
     */
    ec=0;
    for(int ci=0;ci<contours;ci++){
        uint16_t first=(ci==0)?0:(uint16_t)(endPts[ci-1]+1U);
        uint16_t last=endPts[ci];

        Point pfirst=pts[first];
        Point s;
        uint16_t idx;

        if(pfirst.on){
            s=pfirst; idx=first;
        }else{
            Point plast=pts[last];
            if(plast.on) s=plast;
            else s=(Point){(plast.x+pfirst.x)/2,(plast.y+pfirst.y)/2,true};
            idx=first;
        }

        Point current=s;
        uint16_t i=idx;
        bool first_cycle=true;

        while(first_cycle || i!=idx){
            first_cycle=false;
            Point a=pts[i];
            uint16_t n=(i==last)?first:(uint16_t)(i+1U);
            Point b=pts[n];

            if(a.on){
                if(b.on){
                    if(ec<4096)edges[ec++]=(Edge){current.x,current.y,b.x,b.y};
                    current=b;
                    i=n;
                }else{
                    uint16_t nn=(n==last)?first:(uint16_t)(n+1U);
                    Point c=pts[nn];
                    Point end=c.on?c:(Point){(b.x+c.x)/2,(b.y+c.y)/2,true};
                    ec=flatten_quad(edges,ec,4096,current.x,current.y,b.x,b.y,end.x,end.y);
                    current=end;
                    i=c.on?nn:n;
                }
            }else{
                i=n;
                if(i==idx) break;
            }
        }

        if(current.x!=s.x || current.y!=s.y){
            if(ec<4096)edges[ec++]=(Edge){current.x,current.y,s.x,s.y};
        }
    }

    for(uint32_t py=0;py<FONT_HEIGHT;py++){
        for(uint32_t px=0;px<FONT_WIDTH;px++){
            int32_t sx=xMin+(int32_t)(((uint64_t)(px*2U+1U)*w)/(FONT_WIDTH*2U));
            int32_t sy=yMin+(int32_t)(((uint64_t)(py*2U+1U)*h)/(FONT_HEIGHT*2U));
            bool inside=false;
            for(uint32_t e=0;e<ec;e++){
                int32_t ax=edges[e].x0,ay=edges[e].y0,bx=edges[e].x1,by=edges[e].y1;
                if(((ay>sy)!=(by>sy))){
                    int64_t lhs=(int64_t)(sx-ax)*(by-ay);
                    int64_t rhs=(int64_t)(bx-ax)*(sy-ay);
                    if((by>ay&&lhs<rhs)||(by<ay&&lhs>rhs))inside=!inside;
                }
            }
            if(inside) out[py]|=(uint8_t)(1U<<(7U-px));
        }
    }
    return true;
}

static uint16_t glyph_advance(uint16_t glyph)
{
    if (!g_hmtx.off || g_num_hmetrics == 0) return 0;
    uint16_t idx = glyph < g_num_hmetrics ? glyph : (uint16_t)(g_num_hmetrics - 1U);
    return be16(g_ttf + g_hmtx.off + (uint32_t)idx * 4U);
}

void TtfFontInit(void)
{
    g_ttf = _binary_IBMPlexMono_Regular_ttf_start;
    g_ttf_size = (uint32_t)(_binary_IBMPlexMono_Regular_ttf_end - _binary_IBMPlexMono_Regular_ttf_start);

    if (g_ttf_size < 12) return;

    if (!table_ptr("head",&g_head) ||
        !table_ptr("hhea",&g_hhea) ||
        !table_ptr("hmtx",&g_hmtx) ||
        !table_ptr("loca",&g_loca) ||
        !table_ptr("glyf",&g_glyf) ||
        !table_ptr("maxp",&g_maxp) ||
        !table_ptr("cmap",&g_cmap))
        return;

    g_units=be16(g_ttf+g_head.off+18);
    g_loca_format=bes16(g_ttf+g_head.off+50);
    g_num_glyphs=be16(g_ttf+g_maxp.off+4);
    g_num_hmetrics=be16(g_ttf+g_hhea.off+34);

    if(!g_units || !g_num_glyphs || !g_num_hmetrics) return;

    /*
     * Preserve the old fallback glyphs for anything the small kernel
     * rasterizer cannot decode (notably composite glyphs).
     */
    for(uint16_t cp=0;cp<128;cp++){
        uint16_t gid=cmap_lookup(cp);
        if(gid==0) continue;
        uint8_t glyph[FONT_HEIGHT];
        if(raster_glyph(gid,glyph)){
            for(uint32_t y=0;y<FONT_HEIGHT;y++)
                font8x16[cp][y]=glyph[y];
        }
        (void)glyph_advance(gid);
    }
}
