#include "GetSinkSourceInfo.hpp"

#include <iostream>

#include <GDT/GameLoop.hpp>

MfPA::GetSinkSourceInfo::GetSinkSourceInfo(bool getSinkInfo) :
getSinkInfo(getSinkInfo),
operation(nullptr),
run(true)
{
    mainLoop = pa_mainloop_new();
    context = pa_context_new(
        pa_mainloop_get_api(mainLoop), "Get Pulse Sink/Source Info");
    pa_context_set_state_callback(
        context,
        MfPA::GetSinkSourceInfo::get_state_callback,
        this);
    pa_context_connect(context, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
}

MfPA::GetSinkSourceInfo::~GetSinkSourceInfo()
{
    if(operation)
    {
        pa_operation_unref(operation);
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
}

void MfPA::GetSinkSourceInfo::get_state_callback(pa_context* c, void* userdata)
{
    MfPA::GetSinkSourceInfo* getInfo = (MfPA::GetSinkSourceInfo*) userdata;
    switch(pa_context_get_state(c))
    {
    case PA_CONTEXT_UNCONNECTED:
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
        break;
    case PA_CONTEXT_READY:
        if(getInfo->getSinkInfo)
        {
            std::cout << "Available sinks:" << std::endl;
            getInfo->operation = pa_context_get_sink_info_list(
                c,
                MfPA::GetSinkSourceInfo::get_sink_info_callback,
                userdata);
        }
        else
        {
            std::cout << "Available sources:" << std::endl;
            getInfo->operation = pa_context_get_source_info_list(
                c,
                MfPA::GetSinkSourceInfo::get_source_info_callback,
                userdata);
        }
        break;
    case PA_CONTEXT_FAILED:
        std::cerr << "ERROR during context state callback: "
            << pa_strerror(pa_context_errno(c)) << std::endl;
    case PA_CONTEXT_TERMINATED:
        getInfo->run = false;
        break;
    }
}

void MfPA::GetSinkSourceInfo::get_sink_info_callback(
    pa_context* /* c */,
    const pa_sink_info* i,
    int eol,
    void* /* userdata */)
{
//    MfPA::GetSinkSourceInfo* getInfo = (MfPA::GetSinkSourceInfo*) userdata;
    if(eol != PA_OK)
    {
        return;
    }
    std::cout << "  " << i->name << std::endl;
}

void MfPA::GetSinkSourceInfo::get_source_info_callback(
    pa_context* /* c */,
    const pa_source_info* i,
    int eol,
    void* /* userdata */)
{
//    MfPA::GetSinkSourceInfo* getInfo = (MfPA::GetSinkSourceInfo*) userdata;
    if(eol != PA_OK)
    {
        return;
    }
    std::cout << "  " << i->name << std::endl;
}

void MfPA::GetSinkSourceInfo::startMainLoop()
{
    GDT::IntervalBasedGameLoop(
        &run,
        [this] (float /* dt */) {
            pa_mainloop_iterate(mainLoop, 0, nullptr);
            if(operation 
                && pa_operation_get_state(operation) != PA_OPERATION_RUNNING)
            {
                pa_operation_unref(operation);
                operation = nullptr;
                run = false;
            }
        },
        [] () {},
        60,
        1.0f / 120.0f);
}
