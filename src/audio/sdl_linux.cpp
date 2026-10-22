// mixer output through SDL's audio stream
#include <atomic>
#include <vector>

#include <SDL3/SDL.h>

#include "audio/output.h"
#include "core/log.h"

namespace audio {

namespace {

// the console's stereo downmix in the DOLBY image: centre and back pair at -3 dB, no sub
constexpr float kCentreFold = 0.707f;
constexpr float kBackFold = 0.707f;
constexpr uint32_t kFrames = kRate / 100;  // the mixer renders 10 ms at a time

std::atomic<bool> g_surround{false};
std::atomic<bool> g_mono{false};
std::atomic<uint32_t> g_mode{kOutputAuto};
std::atomic<bool> g_mode_changed{false};

struct Output {
    SDL_AudioStream* stream = nullptr;
    int device_channels = 2;
    int channels = 2;  // the stream's input: the 5.1 mix as it is, or folded to stereo
    std::function<void(float*, uint32_t)> render;
    std::vector<float> mix = std::vector<float>(kFrames * kChannels);
    std::vector<float> folded = std::vector<float>(kFrames * 2);
};
Output g_out;

bool WantSurround(uint32_t mode, int device_channels) {
    if (mode == kOutputAuto) return device_channels >= 6;
    return mode == kOutputSurround;
}

const char* OutputName() {
    if (g_mono) return "mono";
    return g_surround ? "5.1" : "stereo";
}

// L = FL + kCentreFold C + kBackFold BL and R = FR + kCentreFold C + kBackFold BR without the sub. Mono hears the
// average of the two on both, as the console's mono downmix.
void Fold(const float* in, float* out, uint32_t frames, bool mono) {
    for (uint32_t i = 0; i < frames; i++, in += kChannels, out += 2) {
        float c = in[Centre] * kCentreFold;
        float l = in[FrontLeft] + c + in[BackLeft] * kBackFold;
        float r = in[FrontRight] + c + in[BackRight] * kBackFold;
        if (mono) l = r = (l + r) * 0.5f;
        out[0] = l;
        out[1] = r;
    }
}

// a 5.1 mix on a device without 5.1 speakers is folded to stereo; SDL places the 5.1 channels on wider layouts
void Apply(Output& o) {
    uint32_t mode = g_mode;
    g_surround = WantSurround(mode, o.device_channels);
    g_mono = mode == kOutputMono;
    o.channels = g_surround && o.device_channels >= 6 ? int(kChannels) : 2;
    SDL_AudioSpec spec{SDL_AUDIO_F32, o.channels, int(kRate)};
    SDL_SetAudioStreamFormat(o.stream, &spec, nullptr);
    LOGI("audio: %s output, %d device channels", OutputName(), o.device_channels);
}

// SDL asks for more: the mixer renders it 10 ms at a time
void SDLCALL Feed(void* user, SDL_AudioStream* stream, int additional, int) {
    Output& o = *static_cast<Output*>(user);
    if (g_mode_changed.exchange(false)) Apply(o);
    while (additional > 0) {
        o.render(o.mix.data(), kFrames);
        const float* data = o.mix.data();
        if (o.channels == 2) {
            Fold(o.mix.data(), o.folded.data(), kFrames, g_mono);
            data = o.folded.data();
        }
        int bytes = int(kFrames * uint32_t(o.channels) * sizeof(float));
        SDL_PutAudioStreamData(stream, data, bytes);
        additional -= bytes;
    }
}

bool Open(Output& o) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
    SDL_AudioSpec device{};
    if (SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &device, nullptr)) o.device_channels = device.channels;
    SDL_AudioSpec spec{SDL_AUDIO_F32, 2, int(kRate)};
    o.stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, Feed, &o);
    if (!o.stream) return false;
    SDL_LockAudioStream(o.stream);
    Apply(o);
    SDL_UnlockAudioStream(o.stream);
    SDL_ResumeAudioStreamDevice(o.stream);
    return true;
}

}  // namespace

bool IsSurround() { return g_surround; }

bool IsMono() { return g_mono; }

void SetOutputMode(uint32_t mode) {
    if (g_mode.exchange(mode) != mode) g_mode_changed = true;
}

void SetPaused(bool paused) {
    if (!g_out.stream) return;
    if (paused) SDL_PauseAudioStreamDevice(g_out.stream);
    else SDL_ResumeAudioStreamDevice(g_out.stream);
}

void StartOutput(std::function<void(float*, uint32_t)> render) {
    g_out.render = std::move(render);
    if (!Open(g_out)) LOGE("audio: SDL audio unavailable, no sound: %s", SDL_GetError());
}

}  // namespace audio
