#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "hsa/hsa.h"
#include "hsa/hsa_ext_amd.h"

#define forjj(n) for(int jj=0; jj<n; ++jj)
#define forii(n) for(int ii=0; ii<n; ++ii)

#define ROC_LOG(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define HSA_CHECK(fn) { hsa_status_t ss = fn; const char* str = 0; \
    if (ss != HSA_STATUS_SUCCESS) { hsa_status_string(ss, &str); ROC_LOG("%s", str); } }

typedef enum {
    ROC_REGION_SYSTEM,
    ROC_REGION_KERNARG,
    ROC_REGION_LOCAL,
    ROC_REGION_GPULOCAL,
    ROC_REGION_COUNT,
} roc_region_e;

typedef struct {
    hsa_agent_t     agent;
    hsa_region_t    regions[ROC_REGION_COUNT];
    char            name[64];
} roc_agent_t;

typedef struct {
    roc_agent_t     cpu;
    roc_agent_t     gpu;
} roc_sys_t;

typedef struct {
    hsa_queue_t*    handle;
    uint32_t        size;
} roc_queue_t;

typedef struct {
    hsa_code_object_t   code_object;
    hsa_executable_t    executable;
} roc_program_t;

hsa_status_t find_agents(hsa_agent_t agent, void *data) {
    roc_sys_t* sys = data;

    hsa_device_type_t hsa_device_type;
    hsa_status_t hsa_error_code = hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &hsa_device_type);
    if (hsa_error_code != HSA_STATUS_SUCCESS) { return hsa_error_code; }

    if (hsa_device_type == HSA_DEVICE_TYPE_GPU) {
        sys->gpu.agent = agent;
        hsa_agent_get_info(agent, HSA_AGENT_INFO_NAME, sys->gpu.name);
    }

    if (hsa_device_type == HSA_DEVICE_TYPE_CPU) {
        sys->cpu.agent = agent;
        hsa_agent_get_info(agent, HSA_AGENT_INFO_NAME, sys->cpu.name);
    }
    return HSA_STATUS_SUCCESS;
}

hsa_status_t find_regions(hsa_region_t region, void* data) {
    hsa_region_segment_t segment_id;
    hsa_region_get_info(region, HSA_REGION_INFO_SEGMENT, &segment_id);

    if (segment_id != HSA_REGION_SEGMENT_GLOBAL) {
        return HSA_STATUS_SUCCESS;
    }

    hsa_region_global_flag_t flags;
    bool host_accessible_region = false;
    hsa_region_get_info(region, HSA_REGION_INFO_GLOBAL_FLAGS, &flags);
    hsa_region_get_info(region, (hsa_region_info_t)HSA_AMD_REGION_INFO_HOST_ACCESSIBLE, &host_accessible_region);

    roc_agent_t* agent = data;

    if (flags & HSA_REGION_GLOBAL_FLAG_FINE_GRAINED) {
        agent->regions[ROC_REGION_SYSTEM] = region;
    }

    if (flags & HSA_REGION_GLOBAL_FLAG_COARSE_GRAINED) {
        if(host_accessible_region){
            agent->regions[ROC_REGION_LOCAL] = region;
        }else{
            agent->regions[ROC_REGION_GPULOCAL] = region;
        }
    }

    if (flags & HSA_REGION_GLOBAL_FLAG_KERNARG) {
        agent->regions[ROC_REGION_KERNARG] = region;
    }

    return HSA_STATUS_SUCCESS;
}

void roc_sys_init(roc_sys_t* sys) {
    hsa_iterate_agents(find_agents   , sys);
    ROC_LOG("AGENT CPU: %s\n", sys->cpu.name);
    ROC_LOG("AGENT GPU: %s\n", sys->gpu.name);

    hsa_agent_iterate_regions(sys->gpu.agent, find_regions , &sys->gpu);

    ROC_LOG("GPU REGION - SYS      : %p\n" , (void*) sys->gpu.regions[0].handle);
    ROC_LOG("GPU REGION - KERNARG  : %p\n" , (void*) sys->gpu.regions[1].handle);
    ROC_LOG("GPU REGION - LOCAL    : %p\n" , (void*) sys->gpu.regions[2].handle);
    ROC_LOG("GPU REGION - GPULOCAL : %p\n" , (void*) sys->gpu.regions[3].handle);
}

void roc_queue_init(roc_queue_t* que, hsa_agent_t agent) {
    hsa_agent_get_info(agent, HSA_AGENT_INFO_QUEUE_MAX_SIZE, &que->size);
    hsa_queue_create(agent, que->size, HSA_QUEUE_TYPE_MULTI, NULL, NULL, UINT32_MAX, UINT32_MAX, &que->handle);
}

void* roc_alloc_memory(roc_agent_t* agent, roc_region_e region, int size) {
    void* ptr = 0;
    HSA_CHECK(hsa_memory_allocate(agent->regions[region], size, &ptr));
    return ptr;
}

uint64_t roc_load_code(roc_agent_t* agent, void* ptr, int size, char* entry_kernel) {
    hsa_code_object_t code_object = {0};
    hsa_executable_t executable   = {0};
    hsa_executable_symbol_t kernel_symbol = {0};

    void* code_mem = roc_alloc_memory(agent, ROC_REGION_SYSTEM, size);
    memcpy(code_mem, ptr, size);

    HSA_CHECK(hsa_code_object_deserialize(code_mem, size, NULL, &code_object));

    HSA_CHECK(hsa_executable_create(HSA_PROFILE_FULL, HSA_EXECUTABLE_STATE_UNFROZEN, NULL, &executable));
    HSA_CHECK(hsa_executable_load_code_object(executable, agent->agent, code_object, NULL));
    HSA_CHECK(hsa_executable_freeze(executable, NULL));
    HSA_CHECK(hsa_executable_get_symbol(executable, NULL, entry_kernel, agent->agent, 0, &kernel_symbol));

    uint64_t code_handle;
    HSA_CHECK(hsa_executable_symbol_get_info(kernel_symbol, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT, &code_handle));
    // HSA_CHECK(hsa_executable_symbol_get_info(kernel_symbol, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_GROUP_SEGMENT_SIZE, &group_static_size);

    return code_handle;
}

typedef struct {
    struct {
        uint16_t x; 
        uint16_t y; 
        uint16_t z; 
    } workgroup_size;

    struct {
        uint32_t x; 
        uint32_t y; 
        uint32_t z; 
    } grid_size;

    void*        kernarg;
    hsa_signal_t signal;
    uint64_t     code;
} roc_dispatch_desc;

void roc_dispatch(roc_queue_t* que, roc_dispatch_desc* dp) {
    hsa_kernel_dispatch_packet_t* aql = 0;
    hsa_queue_t* queue = que->handle;

    const uint32_t queue_mask = que->size - 1;
    uint64_t packet_index = hsa_queue_add_write_index_relaxed(queue, 1);
    aql = (hsa_kernel_dispatch_packet_t*)(queue->base_address) + (packet_index & queue_mask);
    memset((uint8_t*)aql + 4, 0, sizeof(hsa_kernel_dispatch_packet_t) - 4);

    aql->completion_signal    = dp->signal;
    aql->workgroup_size_x     = dp->workgroup_size.x;
    aql->workgroup_size_y     = dp->workgroup_size.y;
    aql->workgroup_size_z     = dp->workgroup_size.z;
    aql->grid_size_x          = dp->grid_size.x;
    aql->grid_size_y          = dp->grid_size.y;
    aql->grid_size_z          = dp->grid_size.z;
    aql->group_segment_size   = 0;
    aql->private_segment_size = 0;
    aql->kernel_object        = dp->code;
    aql->kernarg_address      = dp->kernarg;

    uint16_t header =
        (HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE) |
        (1 << HSA_PACKET_HEADER_BARRIER) |
        (HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE) |
        (HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE);

    uint16_t dim = 1;
    if (aql->grid_size_y > 1)
        dim = 2;
    if (aql->grid_size_z > 1)
        dim = 3;

    uint16_t setup = dim << HSA_KERNEL_DISPATCH_PACKET_SETUP_DIMENSIONS;
    uint32_t header32 = header | (setup << 16);
#if defined(_WIN32) || defined(_WIN64)  // Windows
    _InterlockedExchange(aql, header32);
#else // Linux
    __atomic_store_n((uint32_t*)aql, header32, __ATOMIC_RELEASE);
#endif
    // Ring door bell
    hsa_signal_store_relaxed(que->handle->doorbell_signal, packet_index);
}


void* open_file(char* path, int* _size) {
    struct stat attr = {0};
    stat(path, &attr);
    int size = attr.st_size;

    if (_size) { 
        *_size = size;
    }

    int fd    = open(path, O_RDONLY, 0);
    void* ptr = mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return ptr;
}

uint64_t roc_load_code_from_file(roc_agent_t* agent, char* path, char* entry) {
    int size  = 0;
    void* ptr = open_file(path, &size);
    return roc_load_code(agent, ptr, size, entry); 
}

typedef struct {
    void* ptr;
} test_kernarg_t;

bool roc_wait(hsa_signal_t signal, int timeout) {
    clock_t beg = clock();
    hsa_signal_value_t result;
    do {
        result = hsa_signal_wait_acquire(signal, HSA_SIGNAL_CONDITION_EQ, 0, ~0ULL, HSA_WAIT_STATE_ACTIVE);
        clock_t clocks = clock() - beg;
        if (clocks > (clock_t) timeout * CLOCKS_PER_SEC) {
            ROC_LOG("Kernel execution timed out, elapsed time: %ld", (long) clocks);
            return false;
        }
    } while (result != 0);
    return true;
}

int main(int argc, char** argv) {
    hsa_init();

    roc_sys_t       sys    = {0};
    roc_queue_t     queue  = {0};
    roc_program_t   prg    = {0};
    roc_agent_t*    gpu    = &sys.gpu;

    roc_sys_init(&sys);
    roc_queue_init(&queue, sys.gpu.agent);
    // ROC_LOG("%p", queue.handle->base_address);

    hsa_signal_t signal = {0};
    hsa_signal_create(1, 0, NULL, &signal);

    int MEMSIZE = 1024*1024;

    test_kernarg_t* kernarg = roc_alloc_memory(gpu, ROC_REGION_KERNARG  , MEMSIZE);
    void* cpubuf            = roc_alloc_memory(gpu, ROC_REGION_LOCAL    , MEMSIZE);
    void* gpubuf            = roc_alloc_memory(gpu, ROC_REGION_GPULOCAL , MEMSIZE);

    kernarg->ptr = gpubuf;

    uint64_t code  = roc_load_code_from_file(gpu, "asm.co", "hello_world");

    roc_dispatch_desc dispatch = {
        .workgroup_size = { 64, 1, 1},
        .grid_size      = { 2048, 1, 1},
        .signal         = signal,
        .code           = code,
        .kernarg        = kernarg,
    };

    roc_dispatch(&queue, &dispatch);
    roc_wait(signal, 10);

    HSA_CHECK(hsa_memory_assign_agent(gpubuf, sys.cpu.agent, HSA_ACCESS_PERMISSION_RW));
    HSA_CHECK(hsa_memory_copy(cpubuf, gpubuf, MEMSIZE));

    float* fcpubuf = cpubuf;
    int*   icpubuf = cpubuf;

    forjj(128) {
        forii(16) {
            // ROC_LOG("%2.2f ", fcpubuf[ii]);
            ROC_LOG("%5d ", icpubuf[ii+jj*16]);
        }
        ROC_LOG("\n");
    }

    hsa_shut_down();
}
