#include "cpu.h"
typedef void(*guest_fn)(CPU*);
const uint32_t g_srv_fn_count = 0;
const uint32_t g_srv_fn_addr[1] = {0};
const guest_fn g_srv_fn_ptr[1] = {0};
