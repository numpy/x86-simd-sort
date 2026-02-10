#include "x86simdsort.h"
#include "x86simdsort-internal.h"
#include "x86simdsort-scalar.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <mutex>

#ifdef _MSC_VER
#include <intrin.h>

// MSVC-compatible CPUID wrapper
static void xss_cpuid(int cpuInfo[4], int function_id, int subfunction_id = 0)
{
    __cpuidex(cpuInfo, function_id, subfunction_id);
}

static bool xss_cpu_supports_avx512f()
{
    int cpuInfo[4];
    xss_cpuid(cpuInfo, 0, 0);
    int nIds = cpuInfo[0];
    
    if (nIds < 7) return false;
    
    xss_cpuid(cpuInfo, 7, 0);
    return (cpuInfo[1] & (1 << 16)) != 0; // AVX512F is bit 16 of EBX
}

static bool xss_cpu_supports_avx512dq()
{
    int cpuInfo[4];
    xss_cpuid(cpuInfo, 0, 0);
    int nIds = cpuInfo[0];
    
    if (nIds < 7) return false;
    
    xss_cpuid(cpuInfo, 7, 0);
    return (cpuInfo[1] & (1 << 17)) != 0; // AVX512DQ is bit 17 of EBX
}

static bool xss_cpu_supports_avx512vl()
{
    int cpuInfo[4];
    xss_cpuid(cpuInfo, 0, 0);
    int nIds = cpuInfo[0];
    
    if (nIds < 7) return false;
    
    xss_cpuid(cpuInfo, 7, 0);
    return (cpuInfo[1] & (1 << 31)) != 0; // AVX512VL is bit 31 of EBX
}

static bool xss_cpu_supports_avx512bw()
{
    int cpuInfo[4];
    xss_cpuid(cpuInfo, 0, 0);
    int nIds = cpuInfo[0];
    
    if (nIds < 7) return false;
    
    xss_cpuid(cpuInfo, 7, 0);
    return (cpuInfo[1] & (1 << 30)) != 0; // AVX512BW is bit 30 of EBX
}

static bool xss_cpu_supports_avx512vbmi2()
{
    int cpuInfo[4];
    xss_cpuid(cpuInfo, 0, 0);
    int nIds = cpuInfo[0];
    
    if (nIds < 7) return false;
    
    xss_cpuid(cpuInfo, 7, 0);
    return (cpuInfo[2] & (1 << 6)) != 0; // AVX512VBMI2 is bit 6 of ECX
}

static bool xss_cpu_supports_avx512fp16()
{
    int cpuInfo[4];
    xss_cpuid(cpuInfo, 0, 0);
    int nIds = cpuInfo[0];
    
    if (nIds < 7) return false;
    
    xss_cpuid(cpuInfo, 7, 0);
    return (cpuInfo[3] & (1 << 23)) != 0; // AVX512FP16 is bit 23 of EDX
}

static bool xss_cpu_supports_avx2()
{
    int cpuInfo[4];
    xss_cpuid(cpuInfo, 0, 0);
    int nIds = cpuInfo[0];
    
    if (nIds < 7) return false;
    
    xss_cpuid(cpuInfo, 7, 0);
    return (cpuInfo[1] & (1 << 5)) != 0; // AVX2 is bit 5 of EBX
}

static void xss_cpu_init()
{
    // MSVC doesn't need explicit CPU init
}

#else
// GCC/Clang version using __builtin functions
static void xss_cpu_init()
{
    __builtin_cpu_init();
}

static bool xss_cpu_supports_avx512f()
{
    return __builtin_cpu_supports("avx512f");
}

static bool xss_cpu_supports_avx512dq()
{
    return __builtin_cpu_supports("avx512dq");
}

static bool xss_cpu_supports_avx512vl()
{
    return __builtin_cpu_supports("avx512vl");
}

static bool xss_cpu_supports_avx512bw()
{
    return __builtin_cpu_supports("avx512bw");
}

static bool xss_cpu_supports_avx512vbmi2()
{
    return __builtin_cpu_supports("avx512vbmi2");
}

static bool xss_cpu_supports_avx512fp16()
{
    return __builtin_cpu_supports("avx512fp16");
}

static bool xss_cpu_supports_avx2()
{
    return __builtin_cpu_supports("avx2");
}
#endif

static int check_cpu_feature_support(std::string_view cpufeature)
{
    const char *disable_avx512 = std::getenv("XSS_DISABLE_AVX512");

    if ((cpufeature == "avx512_spr") && (!disable_avx512))
#if defined(__FLT16_MAX__) && !defined(__INTEL_LLVM_COMPILER) \
        && (!defined(__clang_major__) || __clang_major__ >= 18)
        return xss_cpu_supports_avx512f()
                && xss_cpu_supports_avx512fp16()
                && xss_cpu_supports_avx512vbmi2();
#else
        return 0;
#endif
    else if ((cpufeature == "avx512_icl") && (!disable_avx512))
        return xss_cpu_supports_avx512f()
                && xss_cpu_supports_avx512vbmi2()
                && xss_cpu_supports_avx512bw()
                && xss_cpu_supports_avx512vl();
    else if ((cpufeature == "avx512_skx") && (!disable_avx512))
        return xss_cpu_supports_avx512f()
                && xss_cpu_supports_avx512dq()
                && xss_cpu_supports_avx512vl();
    else if (cpufeature == "avx2")
        return xss_cpu_supports_avx2();

    return 0;
}

std::string_view static find_preferred_cpu(
        std::initializer_list<std::string_view> cpulist)
{
    for (auto cpu : cpulist) {
        if (check_cpu_feature_support(cpu)) return cpu;
    }
    return "scalar";
}

constexpr bool
dispatch_requested(std::string_view cpurequested,
                   std::initializer_list<std::string_view> cpulist)
{
    for (auto cpu : cpulist) {
        if (cpu.find(cpurequested) != std::string_view::npos) return true;
    }
    return false;
}

namespace x86simdsort {

#define CAT_(a, b) a##b
#define CAT(a, b) CAT_(a, b)

#define DECLARE_INTERNAL_qsort(TYPE) \
    static void (*internal_qsort##TYPE)(TYPE *, size_t, bool, bool) = NULL; \
    static std::once_flag CAT(CAT(init_flag_, qsort), TYPE); \
    static void CAT(CAT(init_, qsort), TYPE)(); \
    template <> \
    void qsort(TYPE *arr, size_t arrsize, bool hasnan, bool descending) \
    { \
        std::call_once(CAT(CAT(init_flag_, qsort), TYPE), \
                       CAT(CAT(init_, qsort), TYPE)); \
        (*internal_qsort##TYPE)(arr, arrsize, hasnan, descending); \
    }

#define DECLARE_INTERNAL_qselect(TYPE) \
    static void (*internal_qselect##TYPE)(TYPE *, size_t, size_t, bool, bool) \
            = NULL; \
    static std::once_flag CAT(CAT(init_flag_, qselect), TYPE); \
    static void CAT(CAT(init_, qselect), TYPE)(); \
    template <> \
    void qselect( \
            TYPE *arr, size_t k, size_t arrsize, bool hasnan, bool descending) \
    { \
        std::call_once(CAT(CAT(init_flag_, qselect), TYPE), \
                       CAT(CAT(init_, qselect), TYPE)); \
        (*internal_qselect##TYPE)(arr, k, arrsize, hasnan, descending); \
    }

#define DECLARE_INTERNAL_partial_qsort(TYPE) \
    static void (*internal_partial_qsort##TYPE)( \
            TYPE *, size_t, size_t, bool, bool) \
            = NULL; \
    static std::once_flag CAT(CAT(init_flag_, partial_qsort), TYPE); \
    static void CAT(CAT(init_, partial_qsort), TYPE)(); \
    template <> \
    void partial_qsort( \
            TYPE *arr, size_t k, size_t arrsize, bool hasnan, bool descending) \
    { \
        std::call_once(CAT(CAT(init_flag_, partial_qsort), TYPE), \
                       CAT(CAT(init_, partial_qsort), TYPE)); \
        (*internal_partial_qsort##TYPE)(arr, k, arrsize, hasnan, descending); \
    }

#define DECLARE_INTERNAL_argsort(TYPE) \
    static std::vector<size_t> (*internal_argsort##TYPE)( \
            TYPE *, size_t, bool, bool) \
            = NULL; \
    static std::once_flag CAT(CAT(init_flag_, argsort), TYPE); \
    static void CAT(CAT(init_, argsort), TYPE)(); \
    template <> \
    std::vector<size_t> argsort( \
            TYPE *arr, size_t arrsize, bool hasnan, bool descending) \
    { \
        std::call_once(CAT(CAT(init_flag_, argsort), TYPE), \
                       CAT(CAT(init_, argsort), TYPE)); \
        return (*internal_argsort##TYPE)(arr, arrsize, hasnan, descending); \
    }

#define DECLARE_INTERNAL_argselect(TYPE) \
    static std::vector<size_t> (*internal_argselect##TYPE)( \
            TYPE *, size_t, size_t, bool) \
            = NULL; \
    static std::once_flag CAT(CAT(init_flag_, argselect), TYPE); \
    static void CAT(CAT(init_, argselect), TYPE)(); \
    template <> \
    std::vector<size_t> argselect( \
            TYPE *arr, size_t k, size_t arrsize, bool hasnan) \
    { \
        std::call_once(CAT(CAT(init_flag_, argselect), TYPE), \
                       CAT(CAT(init_, argselect), TYPE)); \
        return (*internal_argselect##TYPE)(arr, k, arrsize, hasnan); \
    }

/* simple constexpr function as a way around having #ifdef __FLT16_MAX__ block
 * within the DISPATCH macro */
template <typename T>
constexpr bool IS_TYPE_FLOAT16()
{
#ifdef __FLT16_MAX__
    if constexpr (std::is_same_v<T, _Float16>) { return true; }
#endif
    return false;
}

/* runtime dispatch mechanism */
#ifdef _MSC_VER
// MSVC doesn't support __attribute__((constructor)), so we use lazy initialization
#define DISPATCH(func, TYPE, ISA) \
    DECLARE_INTERNAL_##func(TYPE) \
    static void CAT(CAT(init_, func), TYPE)(void) \
    { \
        CAT(CAT(internal_, func), TYPE) = &xss::scalar::func<TYPE>; \
        xss_cpu_init(); \
        std::string_view preferred_cpu = find_preferred_cpu(ISA); \
        if constexpr (dispatch_requested("avx512", ISA)) { \
            if (preferred_cpu.find("avx512") != std::string_view::npos) { \
                if constexpr (IS_TYPE_FLOAT16<TYPE>()) { \
                    if (preferred_cpu.find("avx512_spr") \
                        != std::string_view::npos) { \
                        CAT(CAT(internal_, func), TYPE) \
                                = &xss::fp16_spr::func<TYPE>; \
                        return; \
                    } \
                    if (preferred_cpu.find("avx512_icl") \
                        != std::string_view::npos) { \
                        CAT(CAT(internal_, func), TYPE) \
                                = &xss::fp16_icl::func<TYPE>; \
                        return; \
                    } \
                } \
                else { \
                    CAT(CAT(internal_, func), TYPE) \
                            = &xss::avx512::func<TYPE>; \
                } \
                return; \
            } \
        } \
        if constexpr (dispatch_requested("avx2", ISA)) { \
            if (preferred_cpu.find("avx2") != std::string_view::npos) { \
                CAT(CAT(internal_, func), TYPE) = &xss::avx2::func<TYPE>; \
                return; \
            } \
        } \
    }
#else
// GCC/Clang version using __attribute__((constructor))
#define DISPATCH(func, TYPE, ISA) \
    DECLARE_INTERNAL_##func(TYPE) static __attribute__((constructor)) void \
    CAT(CAT(init_, func), TYPE)(void) \
    { \
        CAT(CAT(internal_, func), TYPE) = &xss::scalar::func<TYPE>; \
        xss_cpu_init(); \
        std::string_view preferred_cpu = find_preferred_cpu(ISA); \
        if constexpr (dispatch_requested("avx512", ISA)) { \
            if (preferred_cpu.find("avx512") != std::string_view::npos) { \
                if constexpr (IS_TYPE_FLOAT16<TYPE>()) { \
                    if (preferred_cpu.find("avx512_spr") \
                        != std::string_view::npos) { \
                        CAT(CAT(internal_, func), TYPE) \
                                = &xss::fp16_spr::func<TYPE>; \
                        return; \
                    } \
                    if (preferred_cpu.find("avx512_icl") \
                        != std::string_view::npos) { \
                        CAT(CAT(internal_, func), TYPE) \
                                = &xss::fp16_icl::func<TYPE>; \
                        return; \
                    } \
                } \
                else { \
                    CAT(CAT(internal_, func), TYPE) \
                            = &xss::avx512::func<TYPE>; \
                } \
                return; \
            } \
        } \
        if constexpr (dispatch_requested("avx2", ISA)) { \
            if (preferred_cpu.find("avx2") != std::string_view::npos) { \
                CAT(CAT(internal_, func), TYPE) = &xss::avx2::func<TYPE>; \
                return; \
            } \
        } \
    }
#endif

#define ISA_LIST(...) \
    std::initializer_list<std::string_view> \
    { \
        __VA_ARGS__ \
    }

#ifdef __FLT16_MAX__
DISPATCH(qsort, _Float16, ISA_LIST("avx512_spr", "avx512_icl"))
DISPATCH(qselect, _Float16, ISA_LIST("avx512_spr", "avx512_icl"))
DISPATCH(partial_qsort, _Float16, ISA_LIST("avx512_spr", "avx512_icl"))
DISPATCH(argsort, _Float16, ISA_LIST("none"))
DISPATCH(argselect, _Float16, ISA_LIST("none"))
#endif

#define DISPATCH_ALL(func, ISA_16BIT, ISA_32BIT, ISA_64BIT) \
    DISPATCH(func, uint16_t, ISA_16BIT) \
    DISPATCH(func, int16_t, ISA_16BIT) \
    DISPATCH(func, float, ISA_32BIT) \
    DISPATCH(func, int32_t, ISA_32BIT) \
    DISPATCH(func, uint32_t, ISA_32BIT) \
    DISPATCH(func, int64_t, ISA_64BIT) \
    DISPATCH(func, uint64_t, ISA_64BIT) \
    DISPATCH(func, double, ISA_64BIT)

DISPATCH_ALL(qsort,
             (ISA_LIST("avx512_icl")),
             (ISA_LIST("avx512_skx", "avx2")),
             (ISA_LIST("avx512_skx", "avx2")))
DISPATCH_ALL(qselect,
             (ISA_LIST("avx512_icl")),
             (ISA_LIST("avx512_skx", "avx2")),
             (ISA_LIST("avx512_skx", "avx2")))
DISPATCH_ALL(partial_qsort,
             (ISA_LIST("avx512_icl")),
             (ISA_LIST("avx512_skx", "avx2")),
             (ISA_LIST("avx512_skx", "avx2")))
DISPATCH_ALL(argsort,
             (ISA_LIST("none")),
             (ISA_LIST("avx512_skx", "avx2")),
             (ISA_LIST("avx512_skx", "avx2")))
DISPATCH_ALL(argselect,
             (ISA_LIST("none")),
             (ISA_LIST("avx512_skx", "avx2")),
             (ISA_LIST("avx512_skx", "avx2")))

/* Key-Value methods */
#define DECLARE_ALL_KEYVALUE_METHODS(TYPE1, TYPE2) \
    static void(CAT(CAT(*internal_keyvalue_qsort_, TYPE1), TYPE2))( \
            TYPE1 *, TYPE2 *, size_t, bool, bool) \
            = NULL; \
    static void(CAT(CAT(*internal_keyvalue_select_, TYPE1), TYPE2))( \
            TYPE1 *, TYPE2 *, size_t, size_t, bool, bool) \
            = NULL; \
    static void(CAT(CAT(*internal_keyvalue_partial_sort_, TYPE1), TYPE2))( \
            TYPE1 *, TYPE2 *, size_t, size_t, bool, bool) \
            = NULL; \
    static std::once_flag CAT(CAT(CAT(CAT(init_flag_, keyvalue_qsort), _), TYPE1), TYPE2); \
    static std::once_flag CAT(CAT(CAT(CAT(init_flag_, keyvalue_select), _), TYPE1), TYPE2); \
    static std::once_flag CAT(CAT(CAT(CAT(init_flag_, keyvalue_partial_sort), _), TYPE1), TYPE2); \
    static void CAT(CAT(CAT(CAT(init_, keyvalue_qsort), _), TYPE1), TYPE2)(); \
    static void CAT(CAT(CAT(CAT(init_, keyvalue_select), _), TYPE1), TYPE2)(); \
    static void CAT(CAT(CAT(CAT(init_, keyvalue_partial_sort), _), TYPE1), TYPE2)(); \
    template <> \
    void keyvalue_qsort(TYPE1 *key, \
                        TYPE2 *val, \
                        size_t arrsize, \
                        bool hasnan, \
                        bool descending) \
    { \
        std::call_once(CAT(CAT(CAT(CAT(init_flag_, keyvalue_qsort), _), TYPE1), TYPE2), \
                       CAT(CAT(CAT(CAT(init_, keyvalue_qsort), _), TYPE1), TYPE2)); \
        (CAT(CAT(*internal_keyvalue_qsort_, TYPE1), TYPE2))( \
                key, val, arrsize, hasnan, descending); \
    } \
    template <> \
    void keyvalue_select(TYPE1 *key, \
                         TYPE2 *val, \
                         size_t k, \
                         size_t arrsize, \
                         bool hasnan, \
                         bool descending) \
    { \
        std::call_once(CAT(CAT(CAT(CAT(init_flag_, keyvalue_select), _), TYPE1), TYPE2), \
                       CAT(CAT(CAT(CAT(init_, keyvalue_select), _), TYPE1), TYPE2)); \
        (CAT(CAT(*internal_keyvalue_select_, TYPE1), TYPE2))( \
                key, val, k, arrsize, hasnan, descending); \
    } \
    template <> \
    void keyvalue_partial_sort(TYPE1 *key, \
                               TYPE2 *val, \
                               size_t k, \
                               size_t arrsize, \
                               bool hasnan, \
                               bool descending) \
    { \
        std::call_once(CAT(CAT(CAT(CAT(init_flag_, keyvalue_partial_sort), _), TYPE1), TYPE2), \
                       CAT(CAT(CAT(CAT(init_, keyvalue_partial_sort), _), TYPE1), TYPE2)); \
        (CAT(CAT(*internal_keyvalue_partial_sort_, TYPE1), TYPE2))( \
                key, val, k, arrsize, hasnan, descending); \
    }

#ifdef _MSC_VER
#define DISPATCH_KV_FUNC(func, TYPE1, TYPE2, ISA) \
    static void CAT(CAT(CAT(CAT(init_, func), _), TYPE1), TYPE2)(void) \
    { \
        CAT(CAT(CAT(CAT(internal_, func), _), TYPE1), TYPE2) \
                = &xss::scalar::func<TYPE1, TYPE2>; \
        xss_cpu_init(); \
        std::string_view preferred_cpu = find_preferred_cpu(ISA); \
        if constexpr (dispatch_requested("avx512", ISA)) { \
            if (preferred_cpu.find("avx512") != std::string_view::npos) { \
                CAT(CAT(CAT(CAT(internal_, func), _), TYPE1), TYPE2) \
                        = &xss::avx512::func<TYPE1, TYPE2>; \
                return; \
            } \
        } \
        if constexpr (dispatch_requested("avx2", ISA)) { \
            if (preferred_cpu.find("avx2") != std::string_view::npos) { \
                CAT(CAT(CAT(CAT(internal_, func), _), TYPE1), TYPE2) \
                        = &xss::avx2::func<TYPE1, TYPE2>; \
                return; \
            } \
        } \
    }
#else
#define DISPATCH_KV_FUNC(func, TYPE1, TYPE2, ISA) \
    static __attribute__((constructor)) void CAT( \
            CAT(CAT(CAT(init_, func), _), TYPE1), TYPE2)(void) \
    { \
        CAT(CAT(CAT(CAT(internal_, func), _), TYPE1), TYPE2) \
                = &xss::scalar::func<TYPE1, TYPE2>; \
        xss_cpu_init(); \
        std::string_view preferred_cpu = find_preferred_cpu(ISA); \
        if constexpr (dispatch_requested("avx512", ISA)) { \
            if (preferred_cpu.find("avx512") != std::string_view::npos) { \
                CAT(CAT(CAT(CAT(internal_, func), _), TYPE1), TYPE2) \
                        = &xss::avx512::func<TYPE1, TYPE2>; \
                return; \
            } \
        } \
        if constexpr (dispatch_requested("avx2", ISA)) { \
            if (preferred_cpu.find("avx2") != std::string_view::npos) { \
                CAT(CAT(CAT(CAT(internal_, func), _), TYPE1), TYPE2) \
                        = &xss::avx2::func<TYPE1, TYPE2>; \
                return; \
            } \
        } \
    }
#endif

#define DISPATCH_KEYVALUE_SORT(TYPE1, TYPE2, ISA) \
    DECLARE_ALL_KEYVALUE_METHODS(TYPE1, TYPE2) \
    DISPATCH_KV_FUNC(keyvalue_qsort, TYPE1, TYPE2, ISA) \
    DISPATCH_KV_FUNC(keyvalue_select, TYPE1, TYPE2, ISA) \
    DISPATCH_KV_FUNC(keyvalue_partial_sort, TYPE1, TYPE2, ISA)

#define DISPATCH_KEYVALUE_SORT_FORTYPE(type) \
    DISPATCH_KEYVALUE_SORT(type, uint64_t, (ISA_LIST("avx512_skx", "avx2"))) \
    DISPATCH_KEYVALUE_SORT(type, int64_t, (ISA_LIST("avx512_skx", "avx2"))) \
    DISPATCH_KEYVALUE_SORT(type, double, (ISA_LIST("avx512_skx", "avx2"))) \
    DISPATCH_KEYVALUE_SORT(type, uint32_t, (ISA_LIST("avx512_skx", "avx2"))) \
    DISPATCH_KEYVALUE_SORT(type, int32_t, (ISA_LIST("avx512_skx", "avx2"))) \
    DISPATCH_KEYVALUE_SORT(type, float, (ISA_LIST("avx512_skx", "avx2")))

DISPATCH_KEYVALUE_SORT_FORTYPE(uint64_t)
DISPATCH_KEYVALUE_SORT_FORTYPE(int64_t)
DISPATCH_KEYVALUE_SORT_FORTYPE(double)
DISPATCH_KEYVALUE_SORT_FORTYPE(uint32_t)
DISPATCH_KEYVALUE_SORT_FORTYPE(int32_t)
DISPATCH_KEYVALUE_SORT_FORTYPE(float)

} // namespace x86simdsort
//

extern "C" {
XSS_EXPORT_SYMBOL
void keyvalue_qsort_float_uint32(float *key, uint32_t *val, size_t size)
{
    x86simdsort::keyvalue_qsort(key, val, size, true);
}
XSS_EXPORT_SYMBOL
void keyvalue_qsort_float_uint64(float *key, uint64_t *val, size_t size)
{
    x86simdsort::keyvalue_qsort(key, val, size, true);
}
XSS_EXPORT_SYMBOL
void keyvalue_qsort_uint64_uint32(uint64_t *key, uint32_t *val, size_t size)
{
    x86simdsort::keyvalue_qsort(key, val, size, true);
}
XSS_EXPORT_SYMBOL
void keyvalue_qsort_uint64_uint64(uint64_t *key, uint64_t *val, size_t size)
{
    x86simdsort::keyvalue_qsort(key, val, size, true);
}
XSS_EXPORT_SYMBOL
void keyvalue_qsort_int32_uint32(int32_t *key, uint32_t *val, size_t size)
{
    x86simdsort::keyvalue_qsort(key, val, size, true);
}
XSS_EXPORT_SYMBOL
void keyvalue_qsort_int32_uint64(int32_t *key, uint64_t *val, size_t size)
{
    x86simdsort::keyvalue_qsort(key, val, size, true);
}
XSS_EXPORT_SYMBOL
void keyvalue_qsort_uint32_uint32(uint32_t *key, uint32_t *val, size_t size)
{
    x86simdsort::keyvalue_qsort(key, val, size, true);
}
XSS_EXPORT_SYMBOL
void keyvalue_qsort_uint32_uint64(uint32_t *key, uint64_t *val, size_t size)
{
    x86simdsort::keyvalue_qsort(key, val, size, true);
}
}
