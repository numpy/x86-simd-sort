#ifndef X86SIMDSORT_CPUID_H
#define X86SIMDSORT_CPUID_H

#ifdef _MSC_VER
#include <intrin.h>
#include <string>
#include <unordered_map>

static std::unordered_map<std::string, bool> xss_cpu_features;

inline void xss_cpu_init()
{
    int cpuInfo[4] = {0};
    // Check AVX2
    __cpuid(cpuInfo, 0);
    int nIds = cpuInfo[0];
    __cpuid(cpuInfo, 1);
    bool osxsave = (cpuInfo[2] & (1 << 27)) != 0;
    bool avx = (cpuInfo[2] & (1 << 28)) != 0;
    __cpuid(cpuInfo, 7);
    bool avx2 = (cpuInfo[1] & (1 << 5)) != 0;
    bool avx512f = (cpuInfo[1] & (1 << 16)) != 0;
    bool avx512dq = (cpuInfo[1] & (1 << 17)) != 0;
    bool avx512bw = (cpuInfo[1] & (1 << 30)) != 0;
    bool avx512vl = (cpuInfo[1] & (1 << 31)) != 0;
    bool avx512vbmi2 = (cpuInfo[2] & (1 << 6)) != 0;
    bool avx512fp16 = (cpuInfo[3] & (1 << 23)) != 0;
    // Store results
    xss_cpu_features["avx2"] = avx2;
    xss_cpu_features["avx512f"] = avx512f;
    xss_cpu_features["avx512dq"] = avx512dq;
    xss_cpu_features["avx512bw"] = avx512bw;
    xss_cpu_features["avx512vl"] = avx512vl;
    xss_cpu_features["avx512vbmi2"] = avx512vbmi2;
    xss_cpu_features["avx512fp16"] = avx512fp16;
}

inline bool xss_cpu_supports(const char *feature)
{
    auto it = xss_cpu_features.find(feature);
    return it != xss_cpu_features.end() && it->second;
}

#else
#define xss_cpu_init() __builtin_cpu_init()
#define xss_cpu_supports(feature) __builtin_cpu_supports(feature)
#endif // _MSC_VER
#endif // X86SIMDSORT_CPUID_H
