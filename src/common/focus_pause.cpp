// pause on focus loss: the game thread waits while the window is in the background or minimized
#include <atomic>
#include <chrono>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <SDL3/SDL.h>
#endif

#include "app/app.h"
#include "audio/output.h"
#include "common/options.h"
#include "core/log.h"

namespace game {

namespace {

constexpr auto kPoll = std::chrono::milliseconds(20);
#ifndef _WIN32
std::atomic<bool> g_focused{true};
#endif

// a hidden window, behind the splash or for a handover, never pauses: it opens from the game thread
bool Unfocused() {
#ifdef _WIN32
    HWND hwnd = static_cast<HWND>(app::Window());
    if (!hwnd || !IsWindowVisible(hwnd)) return false;
    return GetForegroundWindow() != hwnd || IsIconic(hwnd);
#else
    auto* window = static_cast<SDL_Window*>(app::Window());
    if (!window) return false;
    SDL_WindowFlags flags = SDL_GetWindowFlags(window);
    if (flags & SDL_WINDOW_HIDDEN) return false;
    return !g_focused || (flags & SDL_WINDOW_MINIMIZED);
#endif
}

}  // namespace

#ifndef _WIN32
void SetWindowFocus(bool focused) { g_focused = focused; }
#endif

void WaitWhileUnfocused() {
    if (!Opt().pause_on_focus_loss || !Unfocused()) return;
    LOGI("focus lost: paused");
    g_focus_paused = true;
    audio::SetPaused(true);
    while (Opt().pause_on_focus_loss && Unfocused()) std::this_thread::sleep_for(kPoll);
    audio::SetPaused(false);
    g_focus_pauses++;
    g_focus_paused = false;
    LOGI("focus back: resumed");
}

}  // namespace game
