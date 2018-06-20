#ifndef GET_SINK_SOURCE_INFO_HPP
#define GET_SINK_SOURCE_INFO_HPP

#include <pulse/pulseaudio.h>

namespace MfPA
{

class GetSinkSourceInfo
{
public:
    GetSinkSourceInfo(bool getSinkInfo);
    ~GetSinkSourceInfo();

    static void get_state_callback(pa_context* c, void* userdata);
    static void get_sink_info_callback(
        pa_context* c,
        const pa_sink_info* i,
        int eol,
        void* userdata);
    static void get_source_info_callback(
        pa_context* c,
        const pa_source_info* i,
        int eol,
        void* userdata);

    void startMainLoop();

private:
    bool getSinkInfo;

    pa_mainloop* mainLoop;
    pa_context* context;
    pa_operation* operation;

    bool run;

};

} // namespace MfPA

#endif
