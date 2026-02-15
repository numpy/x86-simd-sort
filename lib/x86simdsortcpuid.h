#ifndef X86SIMDSORT_CPUID_H
#define X86SIMDSORT_CPUID_H

#ifdef _MSC_VER
#include <intrin.h>
#include <string>
#include <unordered_map>

static std::unordered_map<std::string, bool> xss_cpu_features;

static bool os_supports_avx()
{
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);

    bool osxsaveSupported = (cpuInfo[2] & (1 << 27)) != 0; // OSXSAVE bit
    bool avxSupported = (cpuInfo[2] & (1 << 28)) != 0; // AVX bit
    if (!(avxSupported && osxsaveSupported)) return false;

    // Check XCR0[2:1] (XMM and YMM state)
    unsigned long long xcr0 = _xgetbv(0);
    return (xcr0 & 0x6) == 0x6;
}

static bool os_supports_avx512()
{
    if (!os_supports_avx()) return false;

    // Need XCR0[7:5] = opmask/ZMM/YMM state enabled
    unsigned long long xcr0 = _xgetbv(0);
    return (xcr0 & 0xE0) == 0xE0;
}

void xss_cpu_init()
{
    int cpuInfo[4];
    __cpuid(cpuInfo, 0);
    int maxLeaf = cpuInfo[0];

    bool hasAVX2 = false;
    bool hasAVX512F = false, hasAVX512DQ = false, hasAVX512BW = false,
         hasAVX512VL = false;
    bool hasAVX512VBMI2 = false, hasAVX512FP16 = false;

    if (maxLeaf >= 7) {
        __cpuidex(cpuInfo, 7, 0);

        // EBX bits
        hasAVX2 = os_supports_avx() && (cpuInfo[1] & (1 << 5));
        hasAVX512F = os_supports_avx512() && (cpuInfo[1] & (1 << 16));
        hasAVX512DQ = os_supports_avx512() && (cpuInfo[1] & (1 << 17));
        hasAVX512BW = os_supports_avx512() && (cpuInfo[1] & (1 << 30));
        hasAVX512VL = os_supports_avx512() && (cpuInfo[1] & (1 << 31));

        // ECX bits
        hasAVX512VBMI2 = os_supports_avx512() && (cpuInfo[2] & (1 << 6));

        // EDX bits
        hasAVX512FP16 = os_supports_avx512() && (cpuInfo[3] & (1 << 23));
    }

    xss_cpu_features["avx2"] = hasAVX2;
    xss_cpu_features["avx512f"] = hasAVX512F;
    xss_cpu_features["avx512dq"] = hasAVX512DQ;
    xss_cpu_features["avx512bw"] = hasAVX512BW;
    xss_cpu_features["avx512vl"] = hasAVX512VL;
    xss_cpu_features["avx512vbmi2"] = hasAVX512VBMI2;
    xss_cpu_features["avx512fp16"] = hasAVX512FP16;
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
