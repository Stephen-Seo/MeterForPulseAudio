#include "Meter.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>

#include <GDT/GameLoop.hpp>

MfPA::Meter::Meter(const char* sinkOrSourceName, bool isSink) :
currentState(WAITING),
isMonitoringSink(isSink),
sinkOrSourceName(sinkOrSourceName),
stream(nullptr),
runFlag(true)
{
    setenv("PULSE_PROP_application.name", "Meter for PulseAudio", 1);
    setenv("PULSE_PROP_application.icon_name", "multimedia-volume-control", 1);

    pa_context_set_state_callback(
        context,
        MfPA::Meter::get_context_callback,
        this);
    pa_context_connect(context, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
}

MfPA::Meter::~Meter()
{
    if(stream)
    {
        pa_stream_disconnect(stream);
        pa_stream_unref(stream);
    }
}

void MfPA::Meter::get_context_callback(pa_context* c, void* userdata)
{
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
        meter->currentState = MfPA::Meter::PROCESSING;
        if(meter->sinkOrSourceName[0] == '\0')
        {
            // sinkOrSourceName not provided, get default
            pa_operation_unref(pa_context_get_server_info(
                c,
                MfPA::Meter::get_defaults_callback,
                userdata));
        }
        else if(meter->isMonitoringSink)
        {
            // sink provided, getting info on sink
            pa_operation_unref(pa_context_get_sink_info_by_name(
                c,
                meter->sinkOrSourceName,
                MfPA::Meter::get_sink_info_callback,
                userdata));
        }
        else
        {
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
        break;
    case PA_CONTEXT_TERMINATED:
        meter->currentState = MfPA::Meter::TERMINATED;
        break;
    }
}

void MfPA::Meter::get_defaults_callback(
    pa_context* c,
    const pa_server_info* i,
    void* userdata)
{
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
}

void MfPA::Meter::get_sink_info_callback(
    pa_context* c,
    const pa_sink_info* i,
    int eol,
    void* userdata)
{
    MfPA::Meter* meter = (MfPA::Meter*) userdata;
    if(eol != PA_OK)
    {
        meter->currentState = MfPA::Meter::FAILED;
        std::cerr << pa_strerror(eol) << std::endl;
        return;
    }
    // get monitor-source of sink
    pa_operation_unref(pa_context_get_source_info_by_name(
        c,
        i->monitor_source_name,
        MfPA::Meter::get_source_info_callback,
        userdata));
}

void MfPA::Meter::get_source_info_callback(
    pa_context* c,
    const pa_source_info* i,
    int eol,
    void* userdata)
{
    MfPA::Meter* meter = (MfPA::Meter*) userdata;
    if(eol != PA_OK)
    {
        meter->currentState = MfPA::Meter::FAILED;
        std::cerr << pa_strerror(eol) << std::endl;
        return;
    }

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
        PA_STREAM_PEAK_DETECT);
}

void MfPA::Meter::get_stream_state_callback(pa_stream* s, void* userdata)
{
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
        std::cerr << "ERROR: Failed to get stream";
        break;
    case PA_STREAM_TERMINATED:
        meter->currentState = MfPA::Meter::TERMINATED;
        break;
    }
}

void MfPA::Meter::get_stream_data_callback(
    pa_stream* s,
    size_t nbytes,
    void* userdata)
{
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

    {
        std::lock_guard<std::mutex> lock(meter->queueMutex);
        meter->sampleQueue.push(std::move(v));
    }

    pa_stream_drop(s);
}

void MfPA::Meter::startMainLoop()
{
    GDT::IntervalBasedGameLoop(
        &runFlag,
        [this] (float dt) {
            update(dt);
        },
        [this] () {
            draw();
        },
        60,
        1.0f / 120.0f);
}

void MfPA::Meter::update(float dt)
{
    if(currentState == TERMINATED || currentState == FAILED)
    {
        runFlag = false;
    }
}

void MfPA::Meter::draw()
{
}
