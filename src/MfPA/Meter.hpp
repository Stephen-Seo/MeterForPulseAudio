#ifndef METER_FOR_PULSEAUDIO_HPP
#define METER_FOR_PULSEAUDIO_HPP

#define METER_DECAY_RATE 2.0f
#define METER_PREV_DECAY_RATE 1.0f

#include <queue>
#include <vector>

#include <pulse/pulseaudio.h>

#include <SFML/Graphics.hpp>

namespace MfPA
{

class Meter
{
public:
    Meter(
        const char* sinkOrSourceName = "",
        bool isSink = true,
        unsigned int framerateLimit = 0,
        sf::Color barColor = sf::Color::Green
    );
    ~Meter();

    // callbacks required by pulseaudio

    // used for pa_context_set_state_callback
    static void get_context_callback(pa_context* c, void* userdata);
    // used for pa_context_get_server_info
    static void get_defaults_callback(
        pa_context* c,
        const pa_server_info* i,
        void* userdata);
    // used for pa_context_get_sink_info_by_name
    static void get_sink_info_callback(
        pa_context* c,
        const pa_sink_info* i,
        int eol,
        void* userdata);
    // used for pa_context_get_source_info_by_name
    static void get_source_info_callback(
        pa_context* c,
        const pa_source_info* i,
        int eol,
        void* userdata);
    // used for pa_stream_set_state_callback
    static void get_stream_state_callback(pa_stream* s, void* userdata);
    // used for pa_stream_set_read_callback
    static void get_stream_data_callback(
        pa_stream* s,
        size_t nbytes,
        void* userdata);

    void startMainLoop();

private:
    enum CurrentState
    {
        WAITING,
        READY,
        FAILED,
        TERMINATED,
        PROCESSING
    };
    CurrentState currentState;
    bool isMonitoringSink;
    const char* sinkOrSourceName;
    unsigned int framerateLimit;

    bool gotSinkInfo;
    bool gotSourceInfo;

    pa_mainloop* mainLoop;
    pa_context* context;
    pa_stream* stream;

    std::queue<std::vector<float>> sampleQueue;

    bool runFlag;

    unsigned char channels;
    std::vector<float> levels;
    std::vector<std::tuple<float, float>> prevLevels;
    std::vector<bool> levelsChanged;

    sf::RenderWindow window;
    sf::RectangleShape bar;
    sf::Color barColor;

#ifndef NDEBUG
    float levelsPrintTimer;
#endif

    void update(float dt);
    void draw();

};

} // namespace MfPA
#endif
