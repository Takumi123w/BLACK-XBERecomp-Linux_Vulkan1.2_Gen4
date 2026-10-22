#include "common/monitor.h"

#include <algorithm>
#include <cmath>
#include <mutex>

#include <SDL3/SDL.h>

#include "common/aspect.h"
#include "common/options.h"

namespace game {

namespace {

// the setting, copied under the panel's lock
std::string Monitor() {
    std::lock_guard lock(app::UiLock());
    return Opt().monitor;
}

// SDL names a display by its monitor's model; two of the same model are told apart by their number
std::vector<std::pair<SDL_DisplayID, Display>> List() {
    std::vector<std::pair<SDL_DisplayID, Display>> out;
    int count = 0;
    SDL_DisplayID* ids = SDL_GetDisplays(&count);
    if (!ids) return out;
    SDL_DisplayID primary = SDL_GetPrimaryDisplay();
    for (int i = 0; i < count; i++) {
        const SDL_DisplayMode* m = SDL_GetDesktopDisplayMode(ids[i]);
        if (!m) continue;
        float scale = m->pixel_density > 0 ? m->pixel_density : 1.0f;
        const char* name = SDL_GetDisplayName(ids[i]);
        Display d{name && *name ? name : "Display", i + 1,
                  {uint32_t(std::lround(m->w * scale)), uint32_t(std::lround(m->h * scale))}, ids[i] == primary};
        for (const auto& o : out)
            if (o.second.device == d.device) d.device += " (" + std::to_string(d.number) + ")";
        out.emplace_back(ids[i], d);
    }
    SDL_free(ids);
    return out;
}

}  // namespace

std::vector<Display> Displays() {
    std::vector<Display> out;
    for (auto& [id, d] : List()) out.push_back(d);
    return out;
}

// the host goes fullscreen, or centres the window, on the setting's display: Wayland lets no window place itself
void UpdateWindowMonitor(void*) {
    static uint32_t last = 0;
    std::string device = Monitor();
    uint32_t chosen = SDL_GetPrimaryDisplay();
    for (auto& [id, d] : List())
        if (!device.empty() && d.device == device) chosen = id;
    app::SetDisplay(chosen);
    if (last && chosen != last) g_monitor_moves++;
    last = chosen;
}

// the splash opens centred by the window system before the game's settings could move it
void UpdateSplashMonitor() {}

}  // namespace game
