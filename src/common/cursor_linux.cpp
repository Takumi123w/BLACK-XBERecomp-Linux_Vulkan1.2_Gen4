// The game menus' cursor: cursor.png or cursor.bmp beside the exe, untinted, its top-left pixel on the pointer. Its
// visible part is sized, not the image, so padding and resolution don't matter: the longer side is 17 units of the 480
// line page. Without a file the system arrow stays.
#include <algorithm>
#include <cmath>

#include <SDL3/SDL.h>

#include "app/app.h"
#include "common/aspect.h"
#include "common/menu.h"
#include "core/log.h"

namespace app {

namespace {

constexpr float kPageHeight = 480.0f;
constexpr float kVisibleSize = 17.0f;
constexpr uint32_t kVisibleAlpha = 32;  // a fainter pixel is edge fringe, not part of the shape
constexpr const char* kFiles[] = {"cursor.png", "cursor.bmp"};

SDL_Surface* g_image = nullptr;  // straight alpha ARGB
int g_extent = 0;                // the longer side of the visible part, image pixels
bool g_read = false;
SDL_Cursor* g_cursor = nullptr;
SDL_Cursor* g_previous = nullptr;
int g_height = 0;  // the frame height g_cursor is sized for

// the longer side of the visible part's bounds in image pixels, 0 when no pixel is visible
int MeasureVisible(const SDL_Surface* s) {
    int x0 = s->w;
    int y0 = s->h;
    int x1 = 0;
    int y1 = 0;
    for (int y = 0; y < s->h; y++) {
        const auto* row = reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(s->pixels) + size_t(y) * s->pitch);
        for (int x = 0; x < s->w; x++) {
            if (row[x] >> 24 < kVisibleAlpha) continue;
            x0 = std::min(x0, x);
            x1 = std::max(x1, x + 1);
            y0 = std::min(y0, y);
            y1 = std::max(y1, y + 1);
        }
    }
    return x1 > x0 ? std::max(x1 - x0, y1 - y0) : 0;
}

void ReadCursorFile() {
    g_read = true;
    for (const char* name : kFiles) {
        auto path = ExeDir() / name;
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) continue;
        SDL_Surface* loaded = SDL_LoadSurface(path.c_str());
        SDL_Surface* argb = loaded ? SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_ARGB8888) : nullptr;
        if (loaded) SDL_DestroySurface(loaded);
        if (!argb) {
            LOGW("cursor: %s could not be read (%s), trying the next", path.c_str(), SDL_GetError());
            continue;
        }
        g_image = argb;
        int visible = MeasureVisible(g_image);
        g_extent = visible ? visible : std::max(g_image->w, g_image->h);
        LOGI("cursor: %s, %dx%d, visible extent %d", path.c_str(), g_image->w, g_image->h, g_extent);
        return;
    }
}

// the image scaled to the frame, its hot spot the top left pixel
SDL_Cursor* Build(int frame_height) {
    float scale = frame_height * kVisibleSize / kPageHeight / float(g_extent);
    int cw = std::max(1, int(std::lround(g_image->w * scale)));
    int ch = std::max(1, int(std::lround(g_image->h * scale)));
    SDL_Surface* scaled = SDL_ScaleSurface(g_image, cw, ch, SDL_SCALEMODE_LINEAR);
    if (!scaled) return nullptr;
    SDL_Cursor* cursor = SDL_CreateColorCursor(scaled, 0, 0);
    SDL_DestroySurface(scaled);
    return cursor;
}

}  // namespace

void* CursorImage() {
    if (!g_read) ReadCursorFile();
    if (!g_image) return nullptr;
    // the frame is shown in the middle of the window
    Mode window = WindowPixels();
    float aspect = game::Aspect();
    int frame_height = int(std::min(float(window.height), window.width / aspect));
    if (frame_height <= 0) return nullptr;
    if (frame_height != g_height || !g_cursor) {
        SDL_Cursor* cursor = Build(frame_height);
        if (!cursor) return nullptr;
        // the one on screen goes a rebuild later, never while it is shown
        if (g_previous) SDL_DestroyCursor(g_previous);
        g_previous = g_cursor;
        g_cursor = cursor;
        g_height = frame_height;
    }
    return g_cursor;
}

}  // namespace app
