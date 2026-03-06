#include "net.h"
#include "c_api.h"

// "Shim" to expose C functions
// that are missing from the NCNN C_API

extern "C" void ncnn_net_set_lightmode(ncnn_net_t net, int enable)
{
    ((ncnn::Net*)net)->opt.lightmode = (bool)enable;
}
