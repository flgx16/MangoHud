#include "gpu.h"
#include <inttypes.h>

#include <sys/ioctl.h>
#include <libdrm/amdgpu_drm.h>

#include <errno.h>
#include <string.h>

#include "nvctrl.h"
#ifdef HAVE_NVML
#include "nvidia_info.h"
#endif

struct gpuInfo gpu_info;
amdgpu_files amdgpu {};
int amdgpuFD = -1;

bool checkNvidia(const char *pci_dev){
    bool nvSuccess = false;
#ifdef HAVE_NVML
    nvSuccess = checkNVML(pci_dev) && getNVMLInfo();
#endif
#ifdef HAVE_XNVCTRL
    if (!nvSuccess)
        nvSuccess = checkXNVCtrl();
#endif
#ifdef _WIN32
    if (!nvSuccess)
        nvSuccess = checkNVAPI();
#endif
    return nvSuccess;
}

void getNvidiaGpuInfo(){
#ifdef HAVE_NVML
    if (nvmlSuccess){
        getNVMLInfo();
        gpu_info.load = nvidiaUtilization.gpu;
        gpu_info.temp = nvidiaTemp;
        gpu_info.memoryUsed = nvidiaMemory.used / (1024.f * 1024.f * 1024.f);
        gpu_info.CoreClock = nvidiaCoreClock;
        gpu_info.MemClock = nvidiaMemClock;
        gpu_info.powerUsage = nvidiaPowerUsage / 1000;
        gpu_info.memoryTotal = nvidiaMemory.total / (1024.f * 1024.f * 1024.f);
        return;
    }
#endif
#ifdef HAVE_XNVCTRL
    if (nvctrlSuccess) {
        getNvctrlInfo();
        gpu_info.load = nvctrl_info.load;
        gpu_info.temp = nvctrl_info.temp;
        gpu_info.memoryUsed = nvctrl_info.memoryUsed / (1024.f);
        gpu_info.CoreClock = nvctrl_info.CoreClock;
        gpu_info.MemClock = nvctrl_info.MemClock;
        gpu_info.powerUsage = 0;
        gpu_info.memoryTotal = nvctrl_info.memoryTotal;
        return;
    }
#endif
#ifdef _WIN32
nvapi_util();
#endif
}

bool getAmdGpuIOCTLValue(unsigned int query, unsigned int return_size, void* return_pointer) {
    struct drm_amdgpu_info requestBuffer = {};
    requestBuffer.query = query;
    requestBuffer.return_size = return_size;
    requestBuffer.return_pointer = reinterpret_cast<uint64_t>(return_pointer);
    
    int result = ioctl(amdgpuFD, DRM_IOCTL_AMDGPU_INFO, &requestBuffer);
    if (result < 0) printf("%s\n", strerror(errno));

    return result == 0;
}

bool getAmdGpuIOCTLSensorValue(unsigned int sensor, unsigned int return_size, void* return_pointer) {
    struct drm_amdgpu_info requestBuffer = {};
    requestBuffer.query = AMDGPU_INFO_SENSOR;
    requestBuffer.return_size = return_size;
    requestBuffer.return_pointer = reinterpret_cast<uint64_t>(return_pointer);
    requestBuffer.sensor_info.type = sensor;
    
    int result = ioctl(amdgpuFD, DRM_IOCTL_AMDGPU_INFO, &requestBuffer);
    if (result < 0) printf("%s\n", strerror(errno));

    return result == 0;
}

void getAmdGpuIOCTLInfo(){
    getAmdGpuIOCTLSensorValue(AMDGPU_INFO_SENSOR_GPU_LOAD, sizeof(gpu_info.load), &gpu_info.load);

    int temp = 0;
    getAmdGpuIOCTLSensorValue(AMDGPU_INFO_SENSOR_GPU_TEMP, sizeof(temp), &temp);
    gpu_info.temp = temp / 1000;

    getAmdGpuIOCTLSensorValue(AMDGPU_INFO_SENSOR_GFX_SCLK, sizeof(gpu_info.CoreClock), &gpu_info.CoreClock);

    int power = 0;
    getAmdGpuIOCTLSensorValue(AMDGPU_INFO_SENSOR_GPU_AVG_POWER, sizeof(power), &power);
    gpu_info.powerUsage = power / 1000;

    getAmdGpuIOCTLSensorValue(AMDGPU_INFO_SENSOR_GFX_MCLK, sizeof(gpu_info.MemClock), &gpu_info.MemClock);
}

void getAmdGpuSysFSInfo(){
    if (amdgpu.busy) {
        rewind(amdgpu.busy);
        fflush(amdgpu.busy);
        int value = 0;
        if (fscanf(amdgpu.busy, "%d", &value) != 1)
            value = 0;
        gpu_info.load = value;
    }

    if (amdgpu.temp) {
        rewind(amdgpu.temp);
        fflush(amdgpu.temp);
        int value = 0;
        if (fscanf(amdgpu.temp, "%d", &value) != 1)
            value = 0;
        gpu_info.temp = value / 1000;
    }

    int64_t value = 0;

    if (amdgpu.vram_total) {
        rewind(amdgpu.vram_total);
        fflush(amdgpu.vram_total);
        if (fscanf(amdgpu.vram_total, "%" PRId64, &value) != 1)
            value = 0;
        gpu_info.memoryTotal = float(value) / (1024 * 1024 * 1024);
    }

    if (amdgpu.vram_used) {
        rewind(amdgpu.vram_used);
        fflush(amdgpu.vram_used);
        if (fscanf(amdgpu.vram_used, "%" PRId64, &value) != 1)
            value = 0;
        gpu_info.memoryUsed = float(value) / (1024 * 1024 * 1024);
    }

    if (amdgpu.core_clock) {
        rewind(amdgpu.core_clock);
        fflush(amdgpu.core_clock);
        if (fscanf(amdgpu.core_clock, "%" PRId64, &value) != 1)
            value = 0;

        gpu_info.CoreClock = value / 1000000;
    }

    if (amdgpu.memory_clock) {
        rewind(amdgpu.memory_clock);
        fflush(amdgpu.memory_clock);
        if (fscanf(amdgpu.memory_clock, "%" PRId64, &value) != 1)
            value = 0;

        gpu_info.MemClock = value / 1000000;
    }

    if (amdgpu.power_usage) {
        rewind(amdgpu.power_usage);
        fflush(amdgpu.power_usage);
        if (fscanf(amdgpu.power_usage, "%" PRId64, &value) != 1)
            value = 0;

        gpu_info.powerUsage = value / 1000000;
    }
}

void getAmdGpuInfo(){
    if (amdgpuFD < 0) {
        getAmdGpuSysFSInfo();
    } else {
        getAmdGpuIOCTLInfo();
    }
}