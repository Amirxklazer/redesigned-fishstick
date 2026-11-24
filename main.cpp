// ILOT14: HOLLOW ASHES - base game v0.1.0 (rated 14s)
// Switch homebrew, C++ / SDL2 / libnx. One continuous game, auto-save, DLC packs.
#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <dirent.h>
#include <fstream>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>
#include "zones_base.h"
using namespace std;

static const int W = 1280, H = 720, T = 48, HUDY = 672;
static const char* ROOT = "sdmc:/switch/ilot14_hollow";
static const char* VERSION_STR = "0.1.0";

// ---------------------------------------------------------------- data
struct Link { string zone; int x = 0, y = 0; string need, msg; };
struct Zone {
    string id, title;
    int ver = 0, cols = 0;
    bool dark = true;
    vector<string> rows;
    map<char, Link> links;
    map<char, string> notes;
    map<char, pair<string, string>> npcs;
    vector<pair<string, string>> say;
};
static map<string, Zone> zones;
static vector<string> dlcList;

struct Game {
    string zone = "amir_room";
    float px = 1.5f, py = 1.5f, battery = 100, sanity = 100;
    bool light = true;
    set<string> inv, taken, seen;
    double playtime = 0;
    int deaths = 0;
};
static Game G;

struct Stalker { bool on = false; float x = 0, y = 0; double think = 0; vector<pair<int, int>> path; bool chase = false; };
static Stalker ST;

enum Mode { TITLE, PLAY, DEAD, ENDING };
static Mode mode = TITLE;
static deque<pair<string, string>> dlg;
static bool hiding = false, running = false, gQuit = false;
static float hideT = 0, fx = 0, fy = 1, lastX = 1.5f, lastY = 1.5f;
static double clk = 0, autoT = 0, linkCool = 0, deadT = 0, beatT = 0, toastT = 0;
static string toast;

// ---------------------------------------------------------------- helpers
static vector<string> splitStr(const string& s, char d) {
    vector<string> o; string cur;
    for (char c : s) { if (c == d) { o.push_back(cur); cur.clear(); } else cur += c; }
    o.push_back(cur);
    return o;
}
static string trimEnd(string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\n')) s.pop_back();
    return s;
}

static void parseZones(const string& text, int ver) {
    istringstream in(text);
    string raw;
    Zone tmp;
    bool have = false, inMap = false;
    auto commit = [&]() {
        if (!have) return;
        size_t mx = 0;
        for (auto& r : tmp.rows) mx = max(mx, r.size());
        tmp.cols = (int)min<size_t>(mx, 26);
        if (tmp.rows.size() > 14) tmp.rows.resize(14);
        for (auto& r : tmp.rows) r.resize(tmp.cols, '#');
        tmp.ver = ver;
        auto it = zones.find(tmp.id);
        if (it == zones.end() || it->second.ver <= ver) zones[tmp.id] = tmp;
        have = false;
    };
    while (getline(in, raw)) {
        string l = trimEnd(raw);
        if (inMap) {
            if (l == "END") inMap = false;
            else if (have) tmp.rows.push_back(l);
            continue;
        }
        if (l.empty() || l.compare(0, 2, "//") == 0) continue;
        if (l.compare(0, 5, "ZONE ") == 0) {
            commit(); tmp = Zone(); have = true;
            auto p = splitStr(l.substr(5), '|');
            tmp.id = p[0]; tmp.title = p.size() > 1 ? p[1] : p[0];
            continue;
        }
        if (!have) continue;
        if (l == "MAP") { inMap = true; continue; }
        if (l.compare(0, 5, "DARK ") == 0) { tmp.dark = l[5] != '0'; continue; }
        if (l.compare(0, 5, "NOTE ") == 0 && l.size() > 7) { tmp.notes[l[5]] = l.substr(7); continue; }
        if (l.compare(0, 5, "LINK ") == 0) {
            auto p = splitStr(l.substr(5), '|');
            istringstream ls(p[0]);
            string cs; Link k;
            ls >> cs >> k.zone >> k.x >> k.y;
            if (p.size() > 1) k.need = p[1];
            if (p.size() > 2) k.msg = p[2];
            if (!cs.empty()) tmp.links[cs[0]] = k;
            continue;
        }
        if (l.compare(0, 4, "NPC ") == 0) {
            auto p = splitStr(l.substr(4), '|');
            if (p.size() >= 3 && !p[0].empty()) tmp.npcs[p[0][0]] = make_pair(p[1], p[2]);
            continue;
        }
        if (l.compare(0, 4, "SAY ") == 0) {
            auto p = splitStr(l.substr(4), '|');
            if (p.size() >= 2) tmp.say.push_back(make_pair(p[0], p[1]));
            continue;
        }
    }
    commit();
}

struct DlcPack { string id, name; int ver; string text; };
static void loadDlcs() {
    string dir = string(ROOT) + "/dlc";
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    map<string, DlcPack> best;
    while (dirent* e = readdir(d)) {
        string n = e->d_name;
        if (n.size() < 5 || n.substr(n.size() - 4) != ".dlc") continue;
        ifstream f(dir + "/" + n);
        if (!f) continue;
        stringstream ss; ss << f.rdbuf();
        string text = ss.str();
        string first = trimEnd(text.substr(0, text.find('\n')));
        if (first.compare(0, 4, "DLC ") != 0) continue;
        auto p = splitStr(first.substr(4), '|');
        if (p.size() < 3) continue;
        DlcPack x{p[0], p[1], atoi(p[2].c_str()), text};
        auto it = best.find(x.id);
        if (it == best.end() || it->second.ver < x.ver) best[x.id] = x;   // newest version of a pack wins = DLC update
    }
    closedir(d);
    for (auto& kv : best) {
        parseZones(kv.second.text, kv.second.ver + 1);
        dlcList.push_back(kv.second.name + " v" + to_string(kv.second.ver));
    }
}

static Zone& Z() {
    auto it = zones.find(G.zone);
    if (it == zones.end()) it = zones.find("amir_room");
    return it->second;
}
static bool solidCh(char c) {
    return c == '#' || c == ' ' || (c >= 'a' && c <= 'e') || c == 'L' || c == 'N' || c == 'M' || c == 'O' || c == 'Q';
}
static string keyOf(const Zone& z, int x, int y) { return z.id + ":" + to_string(x) + "," + to_string(y); }
static char tileAt(const Zone& z, int x, int y) {
    if (y < 0 || y >= (int)z.rows.size() || x < 0 || x >= z.cols) return '#';
    char c = z.rows[y][x];
    if ((c == 'B' || c == 'K' || c == 'F') && G.taken.count(keyOf(z, x, y))) return '.';
    return c;
}
static bool blocked(float x, float y) {
    const float r = 0.28f;
    const Zone& z = Z();
    for (int dx = -1; dx <= 1; dx += 2)
        for (int dy = -1; dy <= 1; dy += 2)
            if (solidCh(tileAt(z, (int)floorf(x + dx * r), (int)floorf(y + dy * r)))) return true;
    return false;
}
static void say(const string& m) { toast = m; toastT = 2.5; }

// ---------------------------------------------------------------- save
static void saveGame() {
    ofstream f(string(ROOT) + "/save.dat");
    if (!f) return;
    f << "zone " << G.zone << "\npos " << G.px << " " << G.py << "\nbat " << G.battery << "\nsan " << G.sanity
      << "\ntime " << G.playtime << "\ndeaths " << G.deaths << "\n";
    for (auto& s : G.inv) f << "inv " << s << "\n";
    for (auto& s : G.taken) f << "taken " << s << "\n";
    for (auto& s : G.seen) f << "seen " << s << "\n";
}
static bool hasSave() { ifstream f(string(ROOT) + "/save.dat"); return (bool)f; }

static void setupZone() {
    Zone& z = Z();
    ST = Stalker(); hiding = false; linkCool = 0.6;
    for (int r = 0; r < (int)z.rows.size(); r++)
        for (int c = 0; c < z.cols; c++)
            if (z.rows[r][c] == 'S') { ST.on = true; ST.x = c + 0.5f; ST.y = r + 0.5f; }
    lastX = G.px; lastY = G.py;
}
static void enterZone(const string& id, float x, float y) {
    G.zone = id; G.px = x + 0.5f; G.py = y + 0.5f;
    setupZone();
    Zone& z = Z();
    if (!G.seen.count(id)) { G.seen.insert(id); for (auto& s : z.say) dlg.push_back(s); }
    saveGame();
}
static bool loadGame() {
    ifstream f(string(ROOT) + "/save.dat");
    if (!f) return false;
    Game n; string line;
    while (getline(f, line)) {
        istringstream ls(line); string k; ls >> k;
        if (k == "zone") ls >> n.zone;
        else if (k == "pos") ls >> n.px >> n.py;
        else if (k == "bat") ls >> n.battery;
        else if (k == "san") ls >> n.sanity;
        else if (k == "time") ls >> n.playtime;
        else if (k == "deaths") ls >> n.deaths;
        else if (k == "inv") { string s; ls >> s; n.inv.insert(s); }
        else if (k == "taken") { string s; ls >> s; n.taken.insert(s); }
        else if (k == "seen") { string s; ls >> s; n.seen.insert(s); }
    }
    if (!zones.count(n.zone)) { n.zone = "amir_room"; n.px = 1.5f; n.py = 1.5f; }
    G = n;
    setupZone();
    return true;
}
static void newGame() {
    G = Game();
    dlg.clear();
    Zone& z = zones["amir_room"];
    float sx = 1, sy = 1;
    for (int r = 0; r < (int)z.rows.size(); r++)
        for (int c = 0; c < z.cols; c++)
            if (z.rows[r][c] == 'P') { sx = (float)c; sy = (float)r; }
    enterZone("amir_room", sx, sy);
    mode = PLAY;
}
static void die() { if (mode == PLAY) { mode = DEAD; deadT = 0; G.deaths++; } }

// ---------------------------------------------------------------- audio + render helpers
static SDL_Window* WIN; static SDL_Renderer* R;
static TTF_Font *F1, *F2, *F3;
static SDL_Texture* lightTex;
static SDL_AudioDeviceID aud = 0;

static void thump(float v) {
    if (!aud || SDL_GetQueuedAudioSize(aud) > 24000) return;
    static Sint16 b[6000];
    for (int i = 0; i < 6000; i++) {
        float t = i / 48000.f;
        b[i] = (Sint16)(sinf(6.2831853f * 52.f * t) * expf(-t * 16.f) * v * 22000.f);
    }
    SDL_QueueAudio(aud, b, sizeof(b));
}
static void txt(const string& s, int x, int y, SDL_Color c, TTF_Font* f, int align = 0, int wrap = 0) {
    if (s.empty()) return;
    SDL_Surface* su = wrap ? TTF_RenderUTF8_Blended_Wrapped(f, s.c_str(), c, wrap) : TTF_RenderUTF8_Blended(f, s.c_str(), c);
    if (!su) return;
    SDL_Texture* t = SDL_CreateTextureFromSurface(R, su);
    SDL_Rect d = {x, y, su->w, su->h};
    if (align == 1) d.x -= su->w / 2;
    SDL_RenderCopy(R, t, nullptr, &d);
    SDL_FreeSurface(su); SDL_DestroyTexture(t);
}
static void rect(int x, int y, int w, int h, Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) {
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(R, r, g, b, a);
    SDL_Rect d = {x, y, w, h};
    SDL_RenderFillRect(R, &d);
}
static void makeLight() {
    const int N = 256;
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, N, N, 32, SDL_PIXELFORMAT_ARGB8888);
    for (int y = 0; y < N; y++) {
        Uint32* row = (Uint32*)((Uint8*)s->pixels + y * s->pitch);
        for (int x = 0; x < N; x++) {
            float d = hypotf(x - N / 2 + 0.5f, y - N / 2 + 0.5f) / (N / 2);
            float a = d < 0.3f ? 0.f : (d >= 1.f ? 1.f : (d - 0.3f) / 0.7f);
            a = a * a * (3 - 2 * a);
            row[x] = ((Uint32)(245 * a)) << 24;
        }
    }
    lightTex = SDL_CreateTextureFromSurface(R, s);
    SDL_SetTextureBlendMode(lightTex, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(s);
}

// ---------------------------------------------------------------- stalker
static vector<pair<int, int>> bfs(const Zone& z, int sx, int sy, int tx, int ty) {
    int C = z.cols, Rr = (int)z.rows.size();
    vector<pair<int, int>> p;
    if (sx < 0 || sy < 0 || sx >= C || sy >= Rr || tx < 0 || ty < 0 || tx >= C || ty >= Rr) return p;
    vector<int> prev(C * Rr, -2);
    queue<int> q; q.push(sy * C + sx); prev[sy * C + sx] = -1;
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    while (!q.empty()) {
        int c = q.front(); q.pop();
        int cx = c % C, cy = c / C;
        if (cx == tx && cy == ty) break;
        for (int i = 0; i < 4; i++) {
            int nx = cx + dx[i], ny = cy + dy[i];
            if (nx < 0 || ny < 0 || nx >= C || ny >= Rr || prev[ny * C + nx] != -2) continue;
            char ch = z.rows[ny][nx];
            if (solidCh(ch) || (ch >= '0' && ch <= '9') || ch == 'E') continue;
            prev[ny * C + nx] = c; q.push(ny * C + nx);
        }
    }
    if (prev[ty * C + tx] == -2) return p;
    for (int c = ty * C + tx; c != -1; c = prev[c]) p.push_back(make_pair(c % C, c / C));
    reverse(p.begin(), p.end());
    return p;
}
static void stalkerUpdate(double dt) {
    if (!ST.on) return;
    Zone& z = Z();
    float dx = G.px - ST.x, dy = G.py - ST.y, d = sqrtf(dx * dx + dy * dy);
    bool sees = !hiding && (d < 2.8f || (d < 10.f && (G.light || running)));
    if (!hiding && d < 0.65f) { die(); return; }
    if (hiding) {
        if (d < 2.2f) hideT += (float)dt; else hideT = max(0.f, hideT - (float)dt);
        if (hideT > 5.f) { die(); return; }
    }
    ST.think -= dt;
    if (ST.think <= 0) {
        ST.think = 0.3;
        int sx = (int)floorf(ST.x), sy = (int)floorf(ST.y);
        if (sees) {
            auto p = bfs(z, sx, sy, (int)floorf(G.px), (int)floorf(G.py));
            if (!p.empty() && p.size() < 18) { ST.path = p; ST.chase = true; }
            else ST.chase = false;
        } else ST.chase = false;
        if (!ST.chase && ST.path.empty()) {
            for (int t = 0; t < 30; t++) {
                int rx = rand() % z.cols, ry = rand() % (int)z.rows.size();
                char ch = z.rows[ry][rx];
                if (solidCh(ch) || (ch >= '0' && ch <= '9') || ch == 'E') continue;
                auto p = bfs(z, sx, sy, rx, ry);
                if (!p.empty()) { ST.path = p; break; }
            }
        }
    }
    if (!ST.path.empty()) {
        float tx = ST.path.front().first + 0.5f, ty = ST.path.front().second + 0.5f;
        float ddx = tx - ST.x, ddy = ty - ST.y, dd = sqrtf(ddx * ddx + ddy * ddy);
        float sp = (ST.chase ? 3.1f : 1.5f) * (1.f + min(G.deaths, 5) * 0.03f);
        if (dd <= sp * dt) { ST.x = tx; ST.y = ty; ST.path.erase(ST.path.begin()); }
        else { ST.x += ddx / dd * sp * (float)dt; ST.y += ddy / dd * sp * (float)dt; }
    }
    if (d < 9.f) {
        beatT -= dt;
        if (beatT <= 0) { thump(min(1.f, (9.f - d) / 6.f)); beatT = 0.35 + d * 0.1; }
    }
}

// ---------------------------------------------------------------- gameplay
static void unhide() {
    int dxs[4] = {0, 0, 1, -1}, dys[4] = {1, -1, 0, 0};
    Zone& z = Z();
    int tx = (int)floorf(G.px), ty = (int)floorf(G.py);
    for (int i = 0; i < 4; i++)
        if (!solidCh(tileAt(z, tx + dxs[i], ty + dys[i]))) { G.px = tx + dxs[i] + 0.5f; G.py = ty + dys[i] + 0.5f; break; }
    hiding = false; hideT = 0;
}
static void interact() {
    Zone& z = Z();
    int tx = (int)floorf(G.px + fx * 0.8f), ty = (int)floorf(G.py + fy * 0.8f);
    char c = tileAt(z, tx, ty);
    if (c >= 'a' && c <= 'e') {
        auto it = z.notes.find(c);
        dlg.push_back(make_pair(string("Note"), it != z.notes.end() ? it->second : string("...")));
    } else if (c == 'N' || c == 'M' || c == 'O' || c == 'Q') {
        auto it = z.npcs.find(c);
        if (it != z.npcs.end()) dlg.push_back(it->second);
    } else if (c == 'L') {
        hiding = true; hideT = 0; G.px = tx + 0.5f; G.py = ty + 0.5f;
        say("Hiding... press A to come out");
    }
}
static void checkTile() {
    Zone& z = Z();
    int tx = (int)floorf(G.px), ty = (int)floorf(G.py);
    char c = tileAt(z, tx, ty);
    string key = keyOf(z, tx, ty);
    bool special = (c >= '0' && c <= '9') || c == 'E';
    if (!special) { lastX = G.px; lastY = G.py; }
    if (c == 'B') { G.battery = min(100.f, G.battery + 45.f); G.taken.insert(key); say("Battery +45%"); saveGame(); }
    else if (c == 'K') { G.inv.insert("key"); G.taken.insert(key); dlg.push_back(make_pair(string("Amir"), string("The parking key. The tag says 14 CLEANING - P1. It's still warm."))); saveGame(); }
    else if (c == 'F') { G.inv.insert("fuse"); G.taken.insert(key); dlg.push_back(make_pair(string("Amir"), string("A fuse. Gabriel will be happy... if he's still on the radio."))); saveGame(); }
    else if (c == 'E' && linkCool <= 0) {
        if (G.inv.count("fuse")) {
            if (!G.inv.count("ended")) { G.inv.insert("ended"); mode = ENDING; saveGame(); }
            else say("The gate is open. The Lowest Floor waits... (DLC 1)");
        } else say("The exit gate is dead. It needs a fuse.");
        linkCool = 1.5;
    } else if (c >= '0' && c <= '9' && linkCool <= 0) {
        auto it = z.links.find(c);
        if (it == z.links.end()) return;
        Link k = it->second;
        if (!k.need.empty() && !G.inv.count(k.need)) {
            dlg.push_back(make_pair(string("Amir"), k.msg.empty() ? string("It's locked.") : k.msg));
            G.px = lastX; G.py = lastY; linkCool = 1.0;
        } else if (!zones.count(k.zone)) {
            dlg.push_back(make_pair(string("Amir"), k.msg.empty() ? string("This door needs a DLC pack.") : k.msg));
            G.px = lastX; G.py = lastY; linkCool = 1.0;
        } else enterZone(k.zone, (float)k.x, (float)k.y);
    }
}

static void playFrame(double dt, u64 dn, u64 hd, PadState& pad) {
    clk += dt;
    if (!dlg.empty()) { if (dn & HidNpadButton_A) dlg.pop_front(); return; }
    G.playtime += dt; linkCool -= dt; toastT -= dt;
    if (dn & HidNpadButton_Plus) { saveGame(); gQuit = true; return; }
    if (dn & HidNpadButton_X) { if (G.battery > 0 && !hiding) G.light = !G.light; else if (G.battery <= 0) say("The battery is dead."); }
    if (G.battery <= 0) G.light = false;

    running = false;
    if (hiding) {
        if (dn & (HidNpadButton_A | HidNpadButton_B)) unhide();
    } else {
        float mx = 0, my = 0;
        HidAnalogStickState st = padGetStickPos(&pad, 0);
        if (abs(st.x) > 8000) mx = st.x / 32767.f;
        if (abs(st.y) > 8000) my = -st.y / 32767.f;
        if (hd & HidNpadButton_Left) mx = -1;
        if (hd & HidNpadButton_Right) mx = 1;
        if (hd & HidNpadButton_Up) my = -1;
        if (hd & HidNpadButton_Down) my = 1;
        float len = sqrtf(mx * mx + my * my);
        if (len > 1) { mx /= len; my /= len; }
        if (len > 0.1f) {
            fx = mx / len; fy = my / len;
            running = (hd & HidNpadButton_B) != 0;
            float sp = running ? 5.4f : 3.2f;
            float nx = G.px + mx * sp * (float)dt;
            if (!blocked(nx, G.py)) G.px = nx;
            float ny = G.py + my * sp * (float)dt;
            if (!blocked(G.px, ny)) G.py = ny;
        }
        if (dn & HidNpadButton_A) interact();
        checkTile();
    }
    stalkerUpdate(dt);
    if (mode != PLAY) return;

    float dist = ST.on ? hypotf(G.px - ST.x, G.py - ST.y) : 999.f;
    if (G.light && G.battery > 0 && !hiding) G.battery = max(0.f, G.battery - (float)dt * 0.5f);
    if (Z().dark && (!G.light || hiding)) G.sanity -= (float)dt * (hiding ? 0.4f : 1.2f);
    else G.sanity += (float)dt * 0.8f;
    if (dist < 5.f) G.sanity -= (float)dt * (5.f - dist) * 1.2f;
    G.sanity = min(100.f, G.sanity);
    if (G.sanity <= 0) { G.sanity = 0; die(); return; }
    autoT += dt;
    if (autoT > 20) { autoT = 0; saveGame(); }
}

// ---------------------------------------------------------------- drawing
static const SDL_Color ORANGE = {249, 115, 22, 255}, WHITE = {235, 230, 225, 255}, GREY = {150, 140, 135, 255}, RED = {220, 40, 40, 255};

static void drawWorld() {
    Zone& z = Z();
    int rowsN = (int)z.rows.size();
    int ox = (W - z.cols * T) / 2, oy = (HUDY - rowsN * T) / 2;
    int jx = 0, jy = 0;
    if (G.sanity < 30) { float s = (30 - G.sanity) / 30.f * 4.f; jx = (int)((rand() % 7 - 3) * s / 3); jy = (int)((rand() % 7 - 3) * s / 3); }
    for (int y = 0; y < rowsN; y++)
        for (int x = 0; x < z.cols; x++) {
            char c = tileAt(z, x, y);
            int px = ox + x * T + jx, py = oy + y * T + jy;
            if (c == '#' || c == ' ') { rect(px, py, T, T, 62, 36, 32); rect(px, py, T, 6, 90, 52, 44); continue; }
            if ((x + y) & 1) rect(px, py, T, T, 40, 35, 33); else rect(px, py, T, T, 34, 30, 28);
            if (c == 'B') { rect(px + 14, py + 10, 20, 28, 80, 200, 90); rect(px + 19, py + 6, 10, 5, 200, 230, 200); }
            else if (c == 'K') { rect(px + 12, py + 18, 24, 10, 240, 200, 60); rect(px + 12, py + 14, 10, 18, 240, 200, 60); }
            else if (c == 'F') { rect(px + 16, py + 8, 16, 32, 90, 220, 230); }
            else if (c >= '0' && c <= '9') { rect(px + 4, py + 4, T - 8, T - 8, 110, 55, 12); txt(string(1, c), px + T / 2, py + 8, ORANGE, F2, 1); }
            else if (c == 'E') { rect(px + 2, py + 2, T - 4, T - 4, 200, 200, 190); txt("EXIT", px + T / 2, py + 12, {20, 20, 20, 255}, F3, 1); }
            else if (c == 'L') { rect(px + 8, py + 2, T - 16, T - 4, 95, 100, 110); rect(px + 20, py + 18, 8, 3, 30, 30, 30); }
            else if (c >= 'a' && c <= 'e') { rect(px + 4, py + 10, T - 8, T - 20, 110, 75, 45); txt("?", px + T / 2, py + 8, WHITE, F3, 1); }
            else if (c == 'N' || c == 'M' || c == 'O' || c == 'Q') {
                auto it = z.npcs.find(c);
                string nm = it != z.npcs.end() ? it->second.first : "?";
                rect(px + 10, py + 6, T - 20, T - 10, 70, 130, 220);
                txt(nm.substr(0, 1), px + T / 2, py + 10, WHITE, F3, 1);
            }
        }
    if (ST.on) {
        int sx = ox + (int)(ST.x * T) + jx, sy = oy + (int)(ST.y * T) + jy;
        rect(sx - 17, sy - 26, 34, 52, 190, 190, 180);
        rect(sx - 10, sy - 14, 6, 6, 200, 0, 0); rect(sx + 4, sy - 14, 6, 6, 200, 0, 0);
    }
    int cx = ox + (int)(G.px * T) + jx, cy = oy + (int)(G.py * T) + jy;
    if (!hiding) { rect(cx - 13, cy - 13, 26, 26, 249, 115, 22); txt("A", cx, cy - 12, {10, 6, 0, 255}, F3, 1); }

    if (z.dark) {
        float flick = 1.f;
        if (G.battery < 20 && (rand() % 12 == 0)) flick = 0.55f;
        float r = (G.light && !hiding && G.battery > 0) ? (200.f + G.battery * 1.8f) * flick * (1.f + 0.02f * sinf((float)clk * 9)) : 62.f;
        SDL_Rect d = {cx - (int)r, cy - (int)r, (int)(2 * r), (int)(2 * r)};
        SDL_RenderCopy(R, lightTex, nullptr, &d);
        int top = max(0, d.y), bot = min(HUDY, d.y + d.h);
        if (top > 0) rect(0, 0, W, top, 0, 0, 0, 245);
        if (bot < HUDY) rect(0, bot, W, HUDY - bot, 0, 0, 0, 245);
        if (d.x > 0) rect(0, top, d.x, bot - top, 0, 0, 0, 245);
        if (d.x + d.w < W) rect(d.x + d.w, top, W - d.x - d.w, bot - top, 0, 0, 0, 245);
    }
    if (G.sanity < 40) rect(0, 0, W, HUDY, 120, 0, 40, (Uint8)((40 - G.sanity) * 2.2f));
}
static void bar(int x, int y, int w, float v, SDL_Color c, const string& label) {
    txt(label, x, y - 2, GREY, F3);
    rect(x + 52, y + 4, w, 14, 40, 36, 34);
    rect(x + 52, y + 4, (int)(w * v / 100.f), 14, c.r, c.g, c.b);
}
static void drawHud() {
    Zone& z = Z();
    rect(0, HUDY, W, H - HUDY, 14, 10, 8);
    txt(z.title, 14, HUDY + 12, ORANGE, F3);
    bar(430, HUDY + 12, 140, G.battery, {240, 200, 60, 255}, "BAT");
    bar(650, HUDY + 12, 140, G.sanity, {170, 90, 230, 255}, "SAN");
    string items;
    if (G.inv.count("key")) items += "[KEY] ";
    if (G.inv.count("fuse")) items += "[FUSE] ";
    txt(items, 870, HUDY + 12, WHITE, F3);
    txt("14s  |  +: save & quit", W - 14 - 190, HUDY + 12, GREY, F3);
    if (toastT > 0) txt(toast, W / 2, HUDY - 44, ORANGE, F2, 1);
}
static void drawDialog() {
    if (dlg.empty()) return;
    rect(40, 470, W - 80, 190, 10, 6, 6, 235);
    rect(40, 470, W - 80, 4, 249, 115, 22);
    txt(dlg.front().first, 64, 486, ORANGE, F2);
    txt(dlg.front().second, 64, 530, WHITE, F2, 0, W - 160);
    txt("A", W - 90, 624, GREY, F3);
}

int main(int argc, char** argv) {
    mkdir("sdmc:/switch", 0777);
    mkdir(ROOT, 0777);
    mkdir((string(ROOT) + "/dlc").c_str(), 0777);
    plInitialize(PlServiceType_User);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    TTF_Init();
    WIN = SDL_CreateWindow("Ilot14", 0, 0, W, H, SDL_WINDOW_FULLSCREEN);
    R = SDL_CreateRenderer(WIN, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    PlFontData fd;
    plGetSharedFontByType(&fd, PlSharedFontType_Standard);
    F1 = TTF_OpenFontRW(SDL_RWFromMem(fd.address, fd.size), 1, 64);
    F2 = TTF_OpenFontRW(SDL_RWFromMem(fd.address, fd.size), 1, 30);
    F3 = TTF_OpenFontRW(SDL_RWFromMem(fd.address, fd.size), 1, 22);
    SDL_AudioSpec want{}, have{};
    want.freq = 48000; want.format = AUDIO_S16SYS; want.channels = 1; want.samples = 1024;
    aud = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (aud) SDL_PauseAudioDevice(aud, 0);
    makeLight();
    parseZones(BASE_ZONES, 0);
    loadDlcs();
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    bool save = hasSave();
    Uint32 last = SDL_GetTicks();

    while (appletMainLoop() && !gQuit) {
        Uint32 now = SDL_GetTicks();
        double dt = min(0.05, (now - last) / 1000.0);
        last = now;
        padUpdate(&pad);
        u64 dn = padGetButtonsDown(&pad), hd = padGetButtons(&pad);
        SDL_SetRenderDrawColor(R, 8, 6, 6, 255);
        SDL_RenderClear(R);

        if (mode == TITLE) {
            clk += dt;
            txt("ÎLOT14", W / 2, 70, ORANGE, F1, 1);
            txt("HOLLOW ASHES", W / 2, 150, WHITE, F1, 1);
            rect(W / 2 - 60, 250, 120, 56, 249, 115, 22);
            txt("14s", W / 2, 258, {10, 6, 0, 255}, F2, 1);
            txt("Rated 14s: fear, dark themes, implied danger", W / 2, 316, GREY, F3, 1);
            txt(save ? "A  Continue     X  New game (erases save)" : "A  New game", W / 2, 400, WHITE, F2, 1);
            txt("+  Quit", W / 2, 446, GREY, F3, 1);
            string d = "DLC loaded: " + string(dlcList.empty() ? "none" : "");
            for (size_t i = 0; i < dlcList.size(); i++) d += (i ? ", " : "") + dlcList[i];
            txt(d, W / 2, 520, GREY, F3, 1);
            txt(string("Base game v") + VERSION_STR + "  |  one continuous game, auto-saves", W / 2, 650, GREY, F3, 1);
            if (dn & HidNpadButton_A) { if (save) { loadGame(); mode = PLAY; } else newGame(); }
            if ((dn & HidNpadButton_X) && save) newGame();
            if (dn & HidNpadButton_Plus) gQuit = true;
        } else if (mode == PLAY) {
            playFrame(dt, dn, hd, pad);
            if (mode == PLAY || mode == DEAD) { drawWorld(); drawHud(); drawDialog(); }
        } else if (mode == DEAD) {
            deadT += dt;
            drawWorld();
            rect(0, 0, W, H, 90, 0, 0, 200);
            txt("IT FOUND YOU", W / 2, 280, RED, F1, 1);
            txt("Loading your last auto-save...", W / 2, 370, WHITE, F3, 1);
            if (deadT > 2.5) {
                int deaths = G.deaths;
                if (!loadGame()) newGame();
                G.deaths = deaths;
                G.sanity = max(G.sanity, 60.f);
                G.battery = max(G.battery, 25.f);
                mode = PLAY;
            }
        } else if (mode == ENDING) {
            txt("THE GATE OPENS", W / 2, 120, ORANGE, F1, 1);
            txt("Amir, Dalia and Gabriel feel cold air from somewhere far below.", W / 2, 260, WHITE, F2, 1);
            txt("The Ash Thing did not follow. It was waiting at the bottom.", W / 2, 310, WHITE, F2, 1);
            txt("END OF THE BASE GAME", W / 2, 420, GREY, F2, 1);
            txt("More floors arrive as DLC. Door 7 in Parking 1 leads down.", W / 2, 470, GREY, F3, 1);
            txt("A  keep exploring", W / 2, 580, ORANGE, F2, 1);
            if (dn & HidNpadButton_A) mode = PLAY;
        }
        SDL_RenderPresent(R);
    }
    if (mode == PLAY) saveGame();
    if (aud) SDL_CloseAudioDevice(aud);
    TTF_Quit(); SDL_Quit(); plExit();
    return 0;
}
