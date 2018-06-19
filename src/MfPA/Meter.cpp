#include "Meter.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <cmath>

#include <GDT/GameLoop.hpp>

MfPA::Meter::Meter(
    const char* sinkOrSourceName,
    bool isSink,
    unsigned int framerateLimit,
    sf::Color barColor
) :
currentState(WAITING),
isMonitoringSink(isSink),
sinkOrSourceName(sinkOrSourceName),
framerateLimit(framerateLimit),
gotSinkInfo(false),
gotSourceInfo(false),
mainLoop(nullptr),
context(nullptr),
stream(nullptr),
runFlag(true),
channels(1),
window(sf::VideoMode(100,400), "Meter for PulseAudio"),
barColor(barColor)
{
    window.setView(sf::View(sf::FloatRect(0.0f, 0.0f, 1.0f, 1.0f)));
    bar.setFillColor(barColor);

    setenv("PULSE_PROP_application.name", "Meter for PulseAudio", 1);
    setenv("PULSE_PROP_application.icon_name", "multimedia-volume-control", 1);

    mainLoop = pa_mainloop_new();
    context = pa_context_new(pa_mainloop_get_api(mainLoop), "Meter for PulseAudio");
    pa_context_set_state_callback(
        context,
        MfPA::Meter::get_context_callback,
        this);
    pa_context_connect(context, nullptr, PA_CONTEXT_NOFLAGS, nullptr);

#ifndef NDEBUG
    std::cout << "End of Meter constructor" << std::endl;
#endif
}

MfPA::Meter::~Meter()
{
    if(stream)
    {
        pa_stream_disconnect(stream);
        pa_stream_unref(stream);
    }

    if(context)
    {
        pa_context_disconnect(context);
        pa_context_unref(context);
    }

    if(mainLoop)
    {
        pa_mainloop_free(mainLoop);
    }
#ifndef NDEBUG
    std::cout << "End of Meter deconstructor" << std::endl;
#endif
}

void MfPA::Meter::get_context_callback(pa_context* c, void* userdata)
{
#ifndef NDEBUG
    std::cout << "Begin get_context_callback" << std::endl;
#endif
    MfPA::Meter* meter = (MfPA::Meter*) userdata;
    switch(pa_context_get_state(c))
    {
    case PA_CONTEXT_UNCONNECTED:
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
        meter->currentState = MfPA::Meter::WAITING;
        break;
    case PA_CONTEXT_READY:
        if(meter->currentState != MfPA::Meter::WAITING)
        {
#ifndef NDEBUG
            std::cout << "WARNING: Got READY while state is not WAITING"
                << std::endl;
#endif
            break;
        }
        meter->currentState = MfPA::Meter::PROCESSING;
        if(meter->sinkOrSourceName[0] == '\0')
        {
#ifndef NDEBUG
            std::cout << "Attempting to get default" << std::endl;
#endif
            // sinkOrSourceName not provided, get default
            pa_operation_unref(pa_context_get_server_info(
                c,
                MfPA::Meter::get_defaults_callback,
                userdata));
        }
        else if(meter->isMonitoringSink)
        {
#ifndef NDEBUG
            std::cout << "Attempting to get provided sink" << std::endl;
#endif
            // sink provided, getting info on sink
            pa_operation_unref(pa_context_get_sink_info_by_name(
                c,
                meter->sinkOrSourceName,
                MfPA::Meter::get_sink_info_callback,
                userdata));
        }
        else
        {
#ifndef NDEBUG
            std::cout << "Attempting to get provided source" << std::endl;
#endif
            // source provided, getting info on source
            pa_operation_unref(pa_context_get_source_info_by_name(
                c,
                meter->sinkOrSourceName,
                MfPA::Meter::get_source_info_callback,
                userdata));
        }
        break;
    case PA_CONTEXT_FAILED:
        meter->currentState = MfPA::Meter::FAILED;
        std::cerr << pa_strerror(pa_context_errno(meter->context))
            << std::endl;
        break;
    case PA_CONTEXT_TERMINATED:
        meter->currentState = MfPA::Meter::TERMINATED;
        break;
    }
#ifndef NDEBUG
    std::cout << "End get_context_callback" << std::endl;
#endif
}

void MfPA::Meter::get_defaults_callback(
    pa_context* c,
    const pa_server_info* i,
    void* userdata)
{
#ifndef NDEBUG
    std::cout << "Begin get_defaults_callback" << std::endl;
#endif
    MfPA::Meter* meter = (MfPA::Meter*) userdata;
    if(meter->isMonitoringSink)
    {
        meter->sinkOrSourceName = i->default_sink_name;
        // get sink info
        pa_operation_unref(pa_context_get_sink_info_by_name(
            c,
            meter->sinkOrSourceName,
            MfPA::Meter::get_sink_info_callback,
            userdata));
    }
    else
    {
        meter->sinkOrSourceName = i->default_source_name;
        // get source info
        pa_operation_unref(pa_context_get_source_info_by_name(
            c,
            meter->sinkOrSourceName,
            MfPA::Meter::get_source_info_callback,
            userdata));
    }
#ifndef NDEBUG
    std::cout << "End get_defaults_callback" << std::endl;
#endif
}

void MfPA::Meter::get_sink_info_callback(
    pa_context* c,
    const pa_sink_info* i,
    int eol,
    void* userdata)
{
#ifndef NDEBUG
    std::cout << "Begin get_sink_info_callback" << std::endl;
#endif
    MfPA::Meter* meter = (MfPA::Meter*) userdata;
    if(meter->gotSinkInfo)
    {
#ifndef NDEBUG
    std::cout << "Already got sink info, end get_sink_info_callback"
        << std::endl;
#endif
        return;
    }
    if(eol != PA_OK)
    {
        meter->currentState = MfPA::Meter::FAILED;
        std::cerr << "ERROR getting sink info: "
            << pa_strerror(pa_context_errno(c)) << std::endl;
        return;
    }
    // get monitor-source of sink
    pa_operation_unref(pa_context_get_source_info_by_name(
        c,
        i->monitor_source_name,
        MfPA::Meter::get_source_info_callback,
        userdata));
    meter->gotSinkInfo = true;
#ifndef NDEBUG
    std::cout << "End get_sink_info_callback" << std::endl;
#endif
}

void MfPA::Meter::get_source_info_callback(
    pa_context* c,
    const pa_source_info* i,
    int eol,
    void* userdata)
{
#ifndef NDEBUG
    std::cout << "Begin get_source_info_callback" << std::endl;
#endif
    MfPA::Meter* meter = (MfPA::Meter*) userdata;
    if(meter->gotSourceInfo)
    {
#ifndef NDEBUG
    std::cout << "Already got source info, end get_source_info_callback"
        << std::endl;
#endif
        return;
    }
    if(eol != PA_OK)
    {
        meter->currentState = MfPA::Meter::FAILED;
        std::cerr << "ERROR getting source info: "
            << pa_strerror(pa_context_errno(c)) << std::endl;
        return;
    }

    meter->channels = i->sample_spec.channels;
    pa_sample_spec sampleSpec;
    sampleSpec.format = PA_SAMPLE_FLOAT32LE;
    sampleSpec.rate = i->sample_spec.rate;
    sampleSpec.channels = i->sample_spec.channels;
    meter->stream = pa_stream_new(
        c,
        "Meter for PulseAudio stream",
        &sampleSpec,
        &i->channel_map);
    pa_stream_set_state_callback(
        meter->stream,
        MfPA::Meter::get_stream_state_callback,
        userdata);
    pa_stream_set_read_callback(
        meter->stream,
        MfPA::Meter::get_stream_data_callback,
        userdata);
    pa_stream_connect_record(
        meter->stream,
        i->name,
        nullptr,
//        PA_STREAM_NOFLAGS);
        PA_STREAM_PEAK_DETECT);
    meter->gotSourceInfo = true;
#ifndef NDEBUG
    std::cout << "End get_source_info_callback" << std::endl;
#endif
}

void MfPA::Meter::get_stream_state_callback(pa_stream* s, void* userdata)
{
#ifndef NDEBUG
    std::cout << "Begin get_stream_state_callback" << std::endl;
#endif
    MfPA::Meter* meter = (MfPA::Meter*) userdata;
    switch(pa_stream_get_state(s))
    {
    case PA_STREAM_UNCONNECTED:
    case PA_STREAM_CREATING:
        break;
    case PA_STREAM_READY:
        meter->currentState = MfPA::Meter::READY;
        break;
    case PA_STREAM_FAILED:
        meter->currentState = MfPA::Meter::FAILED;
        std::cerr << "ERROR: Failed to get stream, ";
        std::cerr << pa_strerror(pa_context_errno(meter->context))
            << std::endl;
        break;
    case PA_STREAM_TERMINATED:
        meter->currentState = MfPA::Meter::TERMINATED;
        break;
    }
#ifndef NDEBUG
    std::cout << "End get_stream_state_callback" << std::endl;
#endif
}

void MfPA::Meter::get_stream_data_callback(
    pa_stream* s,
    size_t nbytes,
    void* userdata)
{
#ifndef NDEBUG
//    std::cout << "Begin get_stream_data_callback" << std::endl;
#endif
    MfPA::Meter* meter = (MfPA::Meter*) userdata;

    const void* data;
    if(pa_stream_peek(s, &data, &nbytes) < 0)
    {
        std::cerr << pa_strerror(pa_context_errno(meter->context))
            << std::endl;
        return;
    }

    std::vector<float> v(nbytes/sizeof(float));
    memcpy(v.data(), data, nbytes);

    meter->sampleQueue.push(std::move(v));

    pa_stream_drop(s);
#ifndef NDEBUG
//    std::cout << "End get_stream_data_callback" << std::endl;
#endif
}

void MfPA::Meter::startMainLoop()
{
    // initialize levels
    levels.resize(channels);
    prevLevels.resize(channels);
    levelsChanged.resize(channels);
    for(unsigned int i = 0; i < channels; ++i)
    {
        levels[i] = 0.0f;
        prevLevels[i] = std::make_tuple(0.0f, 0.0f);
        levelsChanged[i] = false;
    }

#ifndef NDEBUG
    levelsPrintTimer = 1.0f;
#endif

    GDT::IntervalBasedGameLoop(
        &runFlag,
        [this] (float dt) {
            update(dt);
        },
        [this] () {
            draw();
        },
        framerateLimit,
        1.0f / 120.0f);
}

void MfPA::Meter::update(float dt)
{
    pa_mainloop_iterate(mainLoop, 0, nullptr);
    if(currentState == TERMINATED || currentState == FAILED)
    {
        runFlag = false;
        return;
    }

    {
        sf::Event event;
        while(window.pollEvent(event))
        {
            if(event.type == sf::Event::Closed)
            {
                runFlag = false;
                return;
            }
        }
    }

    for(unsigned int i = 0; i < channels; ++i)
    {
        levelsChanged[i] = false;
    }

    while(!sampleQueue.empty())
    {
        {
            unsigned char currentChannel = 0;
            const auto& front = sampleQueue.front();
            for(auto iter = front.begin(); iter != front.end(); ++iter)
            {
                float fabs = std::abs(*iter);
                if(levels[currentChannel] < fabs)
                {
                    levels[currentChannel] = fabs;
                    levelsChanged[currentChannel] = true;
                }
                if(std::get<0>(prevLevels[currentChannel]) < fabs)
                {
                    std::get<0>(prevLevels[currentChannel]) = fabs;
                    std::get<1>(prevLevels[currentChannel]) = 1.0f;
                }
                currentChannel = (currentChannel + 1) % channels;
            }
        }
        sampleQueue.pop();
    }

    for(unsigned int i = 0; i < channels; ++i)
    {
        if(!levelsChanged[i])
        {
            levels[i] -= METER_DECAY_RATE * dt;
            if(levels[i] < 0.0f)
            {
                levels[i] = 0.0f;
            }
        }
        if(std::get<1>(prevLevels[i]) > 0.0f)
        {
            std::get<1>(prevLevels[i]) -= METER_PREV_DECAY_RATE * dt;
            if(std::get<1>(prevLevels[i]) <= 0.0f)
            {
                std::get<0>(prevLevels[i]) = 0.0f;
                std::get<1>(prevLevels[i]) = 0.0f;
            }
        }
    }

#ifndef NDEBUG
    levelsPrintTimer -= dt;
    if(levelsPrintTimer <= 0.0f)
    {
        for(unsigned int i = 0; i < channels; ++i)
        {
            std::cout << "[" << i << "] " << levels[i] << " ";
        }
        std::cout << std::endl;
        levelsPrintTimer = 1.0f;
    }
#endif
}

void MfPA::Meter::draw()
{
    window.clear();

    for(unsigned int i = 0; i < channels; ++i)
    {
        // prev levels
        barColor.a = 255 * std::get<1>(prevLevels[i]);
        bar.setFillColor(barColor);
        bar.setSize(sf::Vector2f(
            1.0f / (float)channels,
            std::get<0>(prevLevels[i])));
        bar.setPosition(sf::Vector2f(
            (float)i * 1.0f / (float)channels,
            1.0f - std::get<0>(prevLevels[i])));
        window.draw(bar);
        // levels
        barColor.a = 255;
        bar.setFillColor(barColor);
        bar.setSize(sf::Vector2f(
            1.0f / (float)channels,
            levels[i]));
        bar.setPosition(sf::Vector2f(
            (float)i * 1.0f / (float)channels,
            1.0f - levels[i]));
        window.draw(bar);
    }

    window.display();
}
