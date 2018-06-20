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
    sf::Color barColor,
    bool hideMarkings
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
channelsChanged(true),
window(sf::VideoMode(100,400), "Meter for PulseAudio"),
barColor(barColor),
inverted(~barColor.r, ~barColor.g, ~barColor.b),
varray(sf::PrimitiveType::Lines, 2),
hideMarkings(hideMarkings)
{
    window.setView(sf::View(sf::FloatRect(0.0f, 0.0f, 1.0f, 1.0f)));
    bar.setFillColor(barColor);
    if((int)inverted.r + (int)inverted.g + (int)inverted.b < 75)
    {
        inverted.r += (255 - inverted.r) / 1.4;
        inverted.g += (255 - inverted.g) / 1.4;
        inverted.b += (255 - inverted.b) / 1.4;
    }
    varray[0].color = sf::Color(barColor.r, ~barColor.g, ~barColor.b);
    if((int)varray[0].color.r
        + (int)varray[0].color.g
        + (int)varray[0].color.b < 75)
    {
        varray[0].color.r += (255 - varray[0].color.r) / 1.4;
        varray[0].color.g += (255 - varray[0].color.g) / 1.4;
        varray[0].color.b += (255 - varray[0].color.b) / 1.4;
    }
    varray[1].color = varray[0].color;

    setenv("PULSE_PROP_application.name", "Meter for PulseAudio", 1);
    setenv("PULSE_PROP_application.icon_name", "multimedia-volume-control", 1);

    mainLoop = pa_mainloop_new();
    context = pa_context_new(
        pa_mainloop_get_api(mainLoop), "Meter for PulseAudio");
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
    meter->channelsChanged = true;
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
#ifndef NDEBUG
    levelsPrintTimer = 0.0f;
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

MfPA::Meter::Level::Level() :
main(0.0f),
prev(0.0f),
prevTimer(0.0f),
changed(false)
{}

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

    if(channelsChanged)
    {
        levels.resize(channels);
        channelsChanged = false;
    }

    for(unsigned int i = 0; i < channels; ++i)
    {
        levels[i].changed = false;
    }

    while(!sampleQueue.empty())
    {
        {
            const auto& front = sampleQueue.front();
            for(unsigned int i = 0; i < front.size(); ++i)
            {
                float fabs = std::abs(front[i]);
                unsigned char currentChannel = i % channels;
                if(levels[currentChannel].main < fabs)
                {
                    levels[currentChannel].main = fabs;
                    levels[currentChannel].changed = true;
                }
                if(levels[currentChannel].prev <= fabs)
                {
                    levels[currentChannel].prev = fabs;
                    levels[currentChannel].prevTimer = 1.0f;
                }
            }
        }
        sampleQueue.pop();
    }

    for(unsigned int i = 0; i < channels; ++i)
    {
        if(!levels[i].changed)
        {
            levels[i].main -= METER_DECAY_RATE * dt;
            if(levels[i].main < 0.0f)
            {
                levels[i].main = 0.0f;
            }
        }
        if(levels[i].prevTimer > 0.0f)
        {
            levels[i].prevTimer -= METER_PREV_DECAY_RATE * dt;
            if(levels[i].prevTimer <= 0.0f)
            {
                levels[i].prev = 0.0f;
                levels[i].prevTimer = 0.0f;
            }
        }
    }

#ifndef NDEBUG
    levelsPrintTimer -= dt;
    if(levelsPrintTimer <= 0.0f)
    {
        for(unsigned int i = 0; i < channels; ++i)
        {
            std::cout << "[" << i << "] " << levels[i].main << "_"
                << levels[i].prev << " ";
        }
        std::cout << std::endl;
        levelsPrintTimer = 1.0f;
    }
#endif
}

void MfPA::Meter::draw()
{
    window.clear();

    // don't draw until containers have been resized to channel amount
    if(!channelsChanged)
    {
        for(unsigned int i = 0; i < channels; ++i)
        {
            // prev levels
            if(levels[i].prev >= METER_UPPER_LIMIT)
            {
                inverted.a = 255 * levels[i].prevTimer;
                bar.setFillColor(inverted);
            }
            else
            {
                barColor.a = 255 * levels[i].prevTimer;
                bar.setFillColor(barColor);
                barColor.a = 255;
            }
            bar.setSize(sf::Vector2f(
                1.0f / (float)channels,
                levels[i].prev));
            bar.setPosition(sf::Vector2f(
                (float)i * 1.0f / (float)channels,
                1.0f - levels[i].prev));
            window.draw(bar);
            // levels
            bar.setFillColor(barColor);
            bar.setSize(sf::Vector2f(
                1.0f / (float)channels,
                levels[i].main));
            bar.setPosition(sf::Vector2f(
                (float)i * 1.0f / (float)channels,
                1.0f - levels[i].main));
            window.draw(bar);
        }

        if(!hideMarkings)
        {
            // draw lines at 0.75, 0.5, 0.25, and METER_UPPER_LIMIT
            varray[0].position = sf::Vector2f(0.0f, 1.0f - METER_UPPER_LIMIT);
            varray[1].position = sf::Vector2f(1.0f, 1.0f - METER_UPPER_LIMIT);
            window.draw(varray);

            varray[0].position = sf::Vector2f(0.0f, 0.25f);
            varray[1].position = sf::Vector2f(1.0f, 0.25f);
            window.draw(varray);

            varray[0].position = sf::Vector2f(0.0f, 0.5f);
            varray[1].position = sf::Vector2f(1.0f, 0.5f);
            window.draw(varray);

            varray[0].position = sf::Vector2f(0.0f, 0.75f);
            varray[1].position = sf::Vector2f(1.0f, 0.75f);
            window.draw(varray);
        }
    }

    window.display();
}
