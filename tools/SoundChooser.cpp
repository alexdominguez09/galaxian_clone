// tools/SoundChooser.cpp -- interactive sound-design chooser (Stage 20+).
//
// Purpose: lets a human audition every candidate for every game sound and
// record their picks, WITHOUT touching game code. Fully keyboard-driven:
//
//   1..5   preview candidate N          P  play all candidates in order
//   J/K    previous/next sound          ,/. replay last/next candidate
//   ENTER  choose the highlighted candidate for this sound
//   L      list current choices         S  save choices and quit
//   Q      quit without saving          H  help
//
// Output: <choices-file> (default assets/audio/sound_choices.txt) with one
// "<soundId>=<a..e>" line per sound. Re-running the tool loads existing
// choices, so you can redo decisions any time. Adding future sounds =
// extending kCatalog below; everything else adapts.
//
// Build: part of the normal build (target `sound_chooser`). Run it from
// the repository root:  ./build/sound_chooser [choices-file]
#include <SDL2/SDL.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using std::string;
using std::vector;

constexpr int kRate = 44100;

enum class Wave { Square, Saw, Triangle, Sine, Noise };

struct Ctx {
    vector<float> samples;
    float phase = 0.f;
    unsigned noiseState = 0x1234567u;
};

void addTone(Ctx& c, Wave w, float f0, float f1, double seconds, float vol,
             float decayExp, int echoMs = 0, float echoGain = 0.f,
             float vibHz = 0.f, float vibDepth = 0.f)
{
    const int n = static_cast<int>(seconds * kRate);
    const size_t start = c.samples.size();
    c.samples.resize(start + static_cast<size_t>(n), 0.f);
    const unsigned echoDelay = static_cast<unsigned>(echoMs * kRate / 1000);
    for (int i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / n;
        float freq = f0 + (f1 - f0) * t;
        if (vibHz > 0.f) {
            freq += std::sin(2 * 3.14159265f * vibHz * i / kRate) * vibDepth *
                    freq;
        }
        c.phase += freq / kRate;
        c.phase -= std::floor(c.phase);
        float s;
        if (w == Wave::Noise) {
            c.noiseState = c.noiseState * 1664525u + 1013904223u;
            s = static_cast<float>(c.noiseState >> 16) / 32768.f - 1.f;
        } else if (w == Wave::Square) {
            s = c.phase < .5f ? 1.f : -1.f;
        } else if (w == Wave::Saw) {
            s = 2.f * c.phase - 1.f;
        } else if (w == Wave::Triangle) {
            s = 1.f - 4.f * std::fabs(c.phase - .5f);
        } else {
            s = std::sin(2 * 3.14159265f * c.phase);
        }
        const float env = std::pow(1.f - t, decayExp);
        float v = s * env * vol;
        if (echoDelay > 0 && i >= static_cast<int>(echoDelay)) {
            v += c.samples[start + i - echoDelay] * echoGain;
        }
        c.samples[start + static_cast<size_t>(i)] += v;
    }
}

void addSilence(Ctx& c, double seconds)
{
    c.samples.resize(c.samples.size() +
                     static_cast<size_t>(seconds * kRate), 0.f);
}

void warble(Ctx& c, int reps, float top, float bottom, double seg, float vol)
{
    for (int i = 0; i < reps; ++i) {
        addTone(c, Wave::Square, top, bottom, seg, vol, .6f);
    }
}

// ---- the catalog ------------------------------------------------------

struct Candidate {
    char letter;           // 'a'..'e'
    const char* label;
    void (*build)(Ctx&);
};
struct SoundEntry {
    const char* id;        // stable machine id (choices file)
    const char* title;     // pretty display name
    Candidate opt[5];
};

void pfA(Ctx& c){ addTone(c, Wave::Square, 1400, 300, .08f, .6f, 3.f); }
void pfB(Ctx& c){ addTone(c, Wave::Triangle, 1600, 350, .11f, .75f, 2.5f); }
void pfC(Ctx& c){ addTone(c, Wave::Square, 1500, 500, .05f, .55f, 3.f);
                  addTone(c, Wave::Square, 1100, 250, .05f, .55f, 3.f); }
void pfD(Ctx& c){ addTone(c, Wave::Square, 800, 150, .10f, .7f, 2.5f); }
void pfE(Ctx& c){ addTone(c, Wave::Square, 1400, 300, .08f, .55f, 3.f, 70, .30f); }

void efA(Ctx& c){ addTone(c, Wave::Square, 420, 210, .07f, .6f, 2.5f); }
void efB(Ctx& c){ addTone(c, Wave::Square, 520, 260, .09f, .55f, 2.5f, 0,0, 28,.15f); }
void efC(Ctx& c){ addTone(c, Wave::Saw, 600, 180, .09f, .6f, 2.5f); }
void efD(Ctx& c){ addTone(c, Wave::Square, 1200, 600, .05f, .55f, 3.f); }
void efE(Ctx& c){ addTone(c, Wave::Square, 950, 380, .11f, .6f, 2.f); }

void edA(Ctx& c){ addTone(c, Wave::Noise, 0,0,.10f,.8f,2.5f);
                  addTone(c, Wave::Square, 500, 90, .12f, .5f, 2.f); }
void edB(Ctx& c){ addTone(c, Wave::Noise, 0,0,.16f,.75f,2.f); }
void edC(Ctx& c){ addTone(c, Wave::Square, 1300, 200, .08f, .5f, 2.f);
                  addTone(c, Wave::Noise, 0,0,.14f,.6f,2.f); }
void edD(Ctx& c){ addTone(c, Wave::Triangle, 1900, 1900, .10f, .35f, 2.5f);
                  addTone(c, Wave::Noise, 0,0,.14f,.45f,2.f); }
void edE(Ctx& c){ addTone(c, Wave::Noise, 0,0,.14f,.55f,2.5f); }

void pdA(Ctx& c){ addTone(c, Wave::Noise, 0,0,.35f,.95f,1.6f);
                  addTone(c, Wave::Square, 260, 55, .70f, .6f, 1.2f); }
void pdB(Ctx& c){ addTone(c, Wave::Sine, 90, 38, .90f, .95f, .9f);
                  addTone(c, Wave::Noise, 0,0,.60f,.5f,1.6f); }
void pdC(Ctx& c){ addTone(c, Wave::Sine, 1800, 2200, .38f, .35f, .8f);
                  addTone(c, Wave::Noise, 0,0,.50f,.95f,1.4f);
                  addTone(c, Wave::Square, 200, 50, .50f, .55f, 1.2f); }
void pdD(Ctx& c){ for(int k=0;k<3;++k) addTone(c, Wave::Noise,0,0,.16f,.85f-.18f*k,1.8f);
                  addTone(c, Wave::Saw, 300, 40, .45f, .6f, 1.2f); }
void pdE(Ctx& c){ addTone(c, Wave::Saw, 420, 36, 1.0f, .85f, .8f, 0,0, 8,.25f); }

void wsA(Ctx& c){ warble(c, 3, 1300, 420, .13f, .55f); }
void wsB(Ctx& c){ addTone(c, Wave::Square, 420, 1300, .30f, .55f, .8f);
                  addTone(c, Wave::Square, 1300, 420, .30f, .55f, .8f); }
void wsC(Ctx& c){ addTone(c, Wave::Square, 200, 1250, .42f, .55f, .7f); }
void wsD(Ctx& c){ for(int i=0;i<3;++i){ addTone(c, Wave::Square, 500, 1150, .07f, .5f, 2.f); addSilence(c,.03f);} }
void wsE(Ctx& c){ warble(c, 2, 1250, 380, .24f, .55f); }

void gsA(Ctx& c){ const float up[]={392,494,587,659,784,988,1175};
    for(float f:up) addTone(c, Wave::Square,f,f,.055f,.5f,1.2f);
    const float dn[]={988,784,659,494}; for(float f:dn) addTone(c, Wave::Square,f,f,.075f,.5f,1.2f); }
void gsB(Ctx& c){ const float seq[]{392,494,587,784,988,784};
    for(float f:seq) addTone(c, Wave::Square,f,f,.07f,.5f,1.2f); }
void gsC(Ctx& c){ const float up[]={392,494,587,659,784,988};
    for(float f:up) addTone(c, Wave::Square,f,f,.055f,.5f,1.2f);
    addTone(c, Wave::Square, 1175, 1175, .22f, .55f, .9f); }
void gsD(Ctx& c){ addTone(c, Wave::Square, 300, 1500, .18f, .55f, .9f);
                  addTone(c, Wave::Square, 1500, 350, .26f, .55f, .9f); }
void gsE(Ctx& c){ for(int r=0;r<2;++r){ const float up[]={392,494,587,659,784};
    for(float f:up) addTone(c, Wave::Square,f,f,.05f,.48f,1.2f);} }

void goA(Ctx& c){ const float seq[]{659,587,494,392,330};
    for(float f:seq) addTone(c, Wave::Square,f,f,.17f,.55f,1.f); }
void goB(Ctx& c){ addTone(c, Wave::Square, 900, 70, 1.15f, .7f, .5f); }
void goC(Ctx& c){ const float seq[]{587,494,392};
    for(float f:seq) addTone(c, Wave::Square,f,f,.14f,.5f,1.f);
    addTone(c, Wave::Triangle, 98, 98, .70f, .6f, .6f); }
void goD(Ctx& c){ warble(c, 3, 1100, 350, .20f, .5f);
                  addTone(c, Wave::Square, 220, 60, .40f, .55f, 1.f); }
void goE(Ctx& c){ addTone(c, Wave::Square, 392,392,.30f,.6f,1.f); addSilence(c,.08f);
                  addTone(c, Wave::Square, 233,233,.55f,.65f,.8f); }

void muA(Ctx& c){ addTone(c, Wave::Triangle, 55, 55, 3.2f, .18f, .2f, 0,0, .3f, .1f); }
void muB(Ctx& c){ for(int i=0;i<16;++i){ float f=(i%4==2)?98.f:110.f;
    addTone(c, Wave::Square,f,f,.115f,.32f,.5f);
    if(i%2==1) addTone(c, Wave::Noise,0,0,.02f,.12f,5.f);} }
void muC(Ctx& c){ const float arp[]={523,659,784,1047,784,659};
    for(int bar=0;bar<2;++bar){ addTone(c, Wave::Square,131,131,.25f,.3f,.4f);
    for(int i=0;i<6;++i) addTone(c, Wave::Square,arp[i],arp[i],.10f,.2f,1.f);} }
void muD(Ctx& c){ const float arp[]={220,262,330,392,330,262};
    for(int bar=0;bar<2;++bar){ addTone(c, Wave::Saw,87,87,.25f,.3f,.4f);
    for(int i=0;i<6;++i) addTone(c, Wave::Square,arp[i],arp[i],.10f,.2f,1.f);} }
void muE(Ctx& c){ for(int i=0;i<8;++i){ addTone(c, Wave::Square,82,82,.07f,.38f,.4f);
    addSilence(c,.03f);}
    const float lead[]={659,784,880,1047}; for(float f:lead)
    addTone(c, Wave::Square,f,f,.12f,.22f,1.f); }

const SoundEntry kCatalog[] = {
    {"player_fire", "PLAYER FIRE",
     {{'a',"Faithful pew (falling square zap)",pfA},
      {'b',"Bright triangle pew",pfB},
      {'c',"Double zap",pfC},
      {'d',"Low heavy pew",pfD},
      {'e',"Pew + echo tail",pfE}}},
    {"enemy_fire", "ENEMY FIRE",
     {{'a',"Short blip",efA},
      {'b',"Warble blip",efB},
      {'c',"Buzz drop (saw)",efC},
      {'d',"High tick",efD},
      {'e',"Mini dive-cry",efE}}},
    {"enemy_destroyed", "ENEMY DESTROYED",
     {{'a',"Arcade crash (zap into noise)",edA},
      {'b',"Pure noise burst",edB},
      {'c',"Zip-crash",edC},
      {'d',"Metallic clank",edD},
      {'e',"Soft pop",edE}}},
    {"player_destroyed", "PLAYER DESTROYED",
     {{'a',"Faithful long boom",pdA},
      {'b',"Deep sub rumble",pdB},
      {'c',"Whistle-then-boom",pdC},
      {'d',"Stutter bursts + fall",pdD},
      {'e',"Dramatic tremolo fall",pdE}}},
    {"wave_start", "WAVE START",
     {{'a',"Faithful spawn warble x3",wsA},
      {'b',"One long up-down warble",wsB},
      {'c',"Rising sweep",wsC},
      {'d',"Fast triple chirp",wsD},
      {'e',"Slow siren x2",wsE}}},
    {"game_start", "GAME START",
     {{'a',"Boot jingle (run up + down)",gsA},
      {'b',"Condensed jingle",gsB},
      {'c',"Jingle + held high note",gsC},
      {'d',"Smooth double sweep",gsD},
      {'e',"Double rising run",gsE}}},
    {"game_over", "GAME OVER",
     {{'a',"Falling 5-note run",goA},
      {'b',"Long glide down",goB},
      {'c',"Descend into low drone",goC},
      {'d',"Dying warbles + thud",goD},
      {'e',"Two-note da-dum",goE}}},
    {"music_gameplay", "MUSIC (GAMEPLAY LOOP)",
     {{'a',"Authentic silence (soft space hum)",muA},
      {'b',"Minimal bass pulse",muB},
      {'c',"Major arp march",muC},
      {'d',"Minor spy arp",muD},
      {'e',"Intense gallop + lead",muE}}},
};
constexpr int kCatalogCount = static_cast<int>(sizeof(kCatalog) /
                                               sizeof(kCatalog[0]));

// ---- persistence ------------------------------------------------------

bool loadChoices(const string& path, char out[/*kCatalogCount*/])
{
    for (int i = 0; i < kCatalogCount; ++i) {
        out[i] = '-';
    }
    FILE* f = std::fopen(path.c_str(), "r");
    if (f == nullptr) {
        return false;
    }
    char line[256];
    while (std::fgets(line, sizeof(line), f) != nullptr) {
        char id[128];
        char letter;
        if (std::sscanf(line, "%127[^=]=%c", id, &letter) == 2) {
            for (int i = 0; i < kCatalogCount; ++i) {
                if (std::strcmp(id, kCatalog[i].id) == 0) {
                    out[i] = letter;
                }
            }
        }
    }
    std::fclose(f);
    return true;
}

bool saveChoices(const string& path, const char chosen[/*kCatalogCount*/])
{
    FILE* f = std::fopen(path.c_str(), "w");
    if (f == nullptr) {
        return false;
    }
    std::fprintf(f, "# Galaxian sound choices (written by sound_chooser)\n");
    for (int i = 0; i < kCatalogCount; ++i) {
        std::fprintf(f, "%s=%c\n", kCatalog[i].id, chosen[i]);
    }
    std::fclose(f);
    return true;
}

// ---- playback ---------------------------------------------------------

SDL_AudioDeviceID gDevice = 0;
vector<Sint16> gPlaying;
size_t gPos = 0;

void audioCallback(void*, Uint8* stream, int len)
{
    auto* out = reinterpret_cast<Sint16*>(stream);
    const size_t frames = static_cast<size_t>(len) / sizeof(Sint16);
    for (size_t i = 0; i < frames; ++i) {
        if (gPos < gPlaying.size()) {
            out[i] = gPlaying[gPos++];
        } else {
            out[i] = 0;
        }
    }
}

void writeFallbackWav(const Ctx& c);

void play(const Ctx& c)
{
    if (gDevice == 0) {
        writeFallbackWav(c);
        return;
    }
    SDL_LockAudioDevice(gDevice);
    // Master gain with soft clip.
    float peak = 1e-6f;
    for (float v : c.samples) {
        peak = std::max(peak, std::fabs(v));
    }
    const float gain = peak > 1.f ? 0.92f / peak : 1.f;
    gPlaying.clear();
    gPlaying.reserve(c.samples.size());
    for (float v : c.samples) {
        float s = v * gain * 0.8f;
        if (s > 1) s = 1;
        if (s < -1) s = -1;
        gPlaying.push_back(static_cast<Sint16>(s * 32767.f));
    }
    gPos = 0;
    SDL_UnlockAudioDevice(gDevice);
}

Ctx buildCandidate(const SoundEntry& e, int index)
{
    Ctx c;
    e.opt[index].build(c);
    return c;
}

Ctx buildAllSequential(const SoundEntry& e)
{
    Ctx all;
    for (int i = 0; i < 5; ++i) {
        // Marker blips = index count.
        for (int b = 0; b <= i; ++b) {
            addTone(all, Wave::Sine, 1567.f, 1567.f, .06f, .4f, 1.f);
            addSilence(all, .04f);
        }
        addSilence(all, .15f);
        Ctx c = buildCandidate(e, i);
        for (float v : c.samples) all.samples.push_back(v);
        addSilence(all, .35f);
    }
    return all;
}

// ---- ui ---------------------------------------------------------------

void printMenu(int index, char current)
{
    const SoundEntry& e = kCatalog[index];
    std::printf("\n=== [%d/%d] %s (id: %s) ===\n", index + 1, kCatalogCount,
                e.title, e.id);
    for (int i = 0; i < 5; ++i) {
        std::printf("  %d) (%c) %-36s%s\n", i + 1, e.opt[i].letter,
                    e.opt[i].label,
                    current == e.opt[i].letter ? "  <-- CHOSEN" : "");
    }
    std::printf("Keys: 1-5 preview | Enter=choose last previewed | "
                "P=play all | N/B next/back sound\n"
                "      ,/. prev/next candidate | L=list | S=save+quit | "
                "Q=quit | H=help\n");
}

void writeFallbackWav(const Ctx& c)
{
    FILE* f = std::fopen("/tmp/chooser_preview.wav", "wb");
    if (f == nullptr) {
        return;
    }
    float peak = 1e-6f;
    for (float v : c.samples) peak = std::max(peak, std::fabs(v));
    const float gain = peak > 1.f ? 0.92f / peak : 1.f;
    const std::uint32_t dataBytes =
        static_cast<std::uint32_t>(c.samples.size()) * 2;
    auto u32 = [&](std::uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](std::uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f);
    u32(36 + dataBytes);
    std::fwrite("WAVEfmt ", 8, 1, f);
    u32(16); u16(1); u16(1); u32(kRate); u32(kRate * 2); u16(2); u16(16);
    std::fwrite("data", 1, 4, f);
    u32(dataBytes);
    for (float v : c.samples) {
        float s = v * gain;
        if (s > 1) s = 1;
        if (s < -1) s = -1;
        auto i16v = static_cast<std::int16_t>(s * 32767.f);
        std::fwrite(&i16v, 2, 1, f);
    }
    std::fclose(f);
    std::printf("(no device: wrote /tmp/chooser_preview.wav -- play it)\n");
}


int main(int argc, char** argv)
{
    string choicesPath = "assets/audio/sound_choices.txt";
    if (argc > 1) {
        choicesPath = argv[1];
    }

    char chosen[kCatalogCount];
    const bool hadFile = loadChoices(choicesPath, chosen);

    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        std::fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_AudioSpec want{}, have{};
    want.freq = kRate;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = &audioCallback;
    gDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (gDevice == 0) {
        std::fprintf(stderr,
                     "No audio device (%s).\n"
                     "Fallback: previews will be written to "
                     "/tmp/chooser_preview.wav -- play it manually.\n",
                     SDL_GetError());
    } else {
        SDL_PauseAudioDevice(gDevice, 0);
    }

    // Pre-render every candidate once (fast, tiny buffers).
    static Ctx rendered[kCatalogCount][5];
    for (int s = 0; s < kCatalogCount; ++s) {
        for (int o = 0; o < 5; ++o) {
            rendered[s][o] = buildCandidate(kCatalog[s], o);
        }
    }

    std::printf("=== GALAXIAN SOUND CHOOSER ===\n");
    std::printf("Choices file: %s (%s)\n", choicesPath.c_str(),
                hadFile ? "loaded existing" : "starting fresh");
    if (gDevice == 0) {
        std::printf("WARNING: no audio device -- previews go to "
                    "/tmp/chooser_preview.wav\n");
    }

    int soundIndex = 0;
    int lastIndex = 0;
    bool running = true;
    bool dirty = false;
    char line[128];

    printMenu(soundIndex, chosen[soundIndex]);

    while (running) {
        std::printf("> ");
        std::fflush(stdout);
        if (std::fgets(line, sizeof(line), stdin) == nullptr) {
            break;
        }
        char* p = line;
        while (*p == ' ' || *p == '\t') {
            ++p;
        }
        const char cmd = p[0];
        const SoundEntry& e = kCatalog[soundIndex];

        switch (cmd) {
            case '1': case '2': case '3': case '4': case '5': {
                const int idx = cmd - '1';
                play(rendered[soundIndex][idx]);
                lastIndex = idx;
                std::printf("previewing (%c): %s\n", e.opt[idx].letter,
                            e.opt[idx].label);
                break;
            }
            case '\n': case '\r': case '\0':
                if (lastIndex >= 0 && lastIndex < 5) {
                    chosen[soundIndex] = e.opt[lastIndex].letter;
                    dirty = true;
                    std::printf("CHOSEN %s = %c\n", e.id, chosen[soundIndex]);
                } else {
                    std::printf("preview a candidate first (1-5)\n");
                }
                break;
            case 'P': case 'p':
                std::printf("playing all candidates (blips mark each):\n");
                play(buildAllSequential(e));
                break;
            case 'N': case 'n':
                soundIndex = (soundIndex + 1) % kCatalogCount;
                lastIndex = 0;
                printMenu(soundIndex, chosen[soundIndex]);
                break;
            case 'B': case 'b':
                soundIndex =
                    (soundIndex + kCatalogCount - 1) % kCatalogCount;
                lastIndex = 0;
                printMenu(soundIndex, chosen[soundIndex]);
                break;
            case ',': {
                const int prev = lastIndex - 1 < 0 ? 4 : lastIndex - 1;
                play(rendered[soundIndex][prev]);
                lastIndex = prev;
                std::printf("previewing (%c): %s\n", e.opt[prev].letter,
                            e.opt[prev].label);
                break;
            }
            case '.': {
                const int next = (lastIndex + 1) % 5;
                play(rendered[soundIndex][next]);
                lastIndex = next;
                std::printf("previewing (%c): %s\n", e.opt[next].letter,
                            e.opt[next].label);
                break;
            }
            case 'L': case 'l':
                std::printf("--- current choices ---\n");
                for (int i = 0; i < kCatalogCount; ++i) {
                    std::printf("  %-20s = %c\n", kCatalog[i].id, chosen[i]);
                }
                break;
            case 'S': case 's':
                if (saveChoices(choicesPath, chosen)) {
                    std::printf("saved -> %s\n", choicesPath.c_str());
                } else {
                    std::fprintf(stderr, "FAILED to save %s\n",
                                 choicesPath.c_str());
                }
                running = false;
                break;
            case 'Q': case 'q':
                std::printf(dirty ? "quit WITHOUT saving (S saves)\n"
                                  : "bye\n");
                running = false;
                break;
            case 'H': case 'h':
                std::printf("1-5 preview | Enter=choose last | P=play all | "
                            "N/B=next/back | ,.=prev/next candidate\n"
                            "L=list | S=save+quit | Q=quit\n");
                break;
            default:
                std::printf("? (H for help)\n");
                break;
        }
    }

    if (gDevice != 0) {
        SDL_CloseAudioDevice(gDevice);
    }
    SDL_Quit();
    return 0;
}
