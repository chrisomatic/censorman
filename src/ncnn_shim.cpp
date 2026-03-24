#include "net.h"
#include "c_api.h"

// "Shim" to expose C functions
// that are missing from the NCNN C_API

extern "C" void ncnn_net_set_lightmode(ncnn_net_t net, int enable)
{
    ((ncnn::Net*)net)->opt.lightmode = (bool)enable;
}

extern "C" void ncnn_extractor_clear(ncnn_extractor_t ex)
{
    ((ncnn::Extractor*)ex)->clear();
}

extern "C" void ncnn_net_set_workspace_allocator(ncnn_net_t net) {
    ncnn::Net *n = (ncnn::Net*)net;
    n->opt.workspace_allocator = new ncnn::PoolAllocator();
    n->opt.blob_allocator = new ncnn::PoolAllocator();
}
