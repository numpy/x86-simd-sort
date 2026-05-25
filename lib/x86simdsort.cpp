#if defined(_MSC_VER)
#define XSS_ATTRIBUTE_CONSTRUCTOR
#else
#define XSS_ATTRIBUTE_CONSTRUCTOR __attribute__((constructor))
#endif
#include "x86simdsort.h"
#include "x86simdsort-internal.h"
#include "x86simdsort-scalar.h"
#include "x86simdsortcpuid.h"
#include <algorithm>
#include <iostream>
#include <string>

static int check_cpu_feature_support(std::string_view cpufeature)
{
    const char *disable_avx512 = std::getenv("XSS_DISABLE_AVX512");

    if ((cpufeature == "avx512_spr") && (!disable_avx512))
#if defined(__FLT16_MAX__) && !defined(__INTEL_LLVM_COMPILER) \
        && (!defined(__clang_major__) || __clang_major__ >= 18)
        return xss_cpu_supports("avx512f") && xss_cpu_supports("avx512fp16")
                && xss_cpu_supports("avx512vbmi2");
#else
        return 0;
#endif
    else if ((cpufeature == "avx512_icl") && (!disable_avx512))
        return xss_cpu_supports("avx512f") && xss_cpu_supports("avx512vbmi2")
                && xss_cpu_supports("avx512bw") && xss_cpu_supports("avx512vl");
    else if ((cpufeature == "avx512_skx") && (!disable_avx512))
        return xss_cpu_supports("avx512f") && xss_cpu_supports("avx512dq")
                && xss_cpu_supports("avx512vl");
    else if (cpufeature == "avx2")
        return xss_cpu_supports("avx2");

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

/* runtime dispatch mechanism */
#define DISPATCH(func, TYPE, ISA) \
    DECLARE_INTERNAL_##func(TYPE) static XSS_ATTRIBUTE_CONSTRUCTOR void CAT( \
            CAT(resolve_, func), TYPE)(void) \
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

#ifdef _MSC_VER
#define DECLARE_INTERNAL_qsort(TYPE) \
    static void CAT(resolve_qsort, TYPE)(void); \
    static void (*internal_qsort##TYPE)(TYPE *, size_t, bool, bool, bool) \
            = NULL; \
    template <> \
    void XSS_EXPORT_SYMBOL qsort(TYPE *arr, \
                                 size_t arrsize, \
                                 bool hasnan, \
                                 bool descending, \
                                 bool trailing_nans) \
    { \
        if (internal_qsort##TYPE == NULL) { CAT(resolve_qsort, TYPE)(); } \
        (*internal_qsort##TYPE)( \
                arr, arrsize, hasnan, descending, trailing_nans); \
    }

#define DECLARE_INTERNAL_qselect(TYPE) \
    static void CAT(resolve_qselect, TYPE)(void); \
    static void (*internal_qselect##TYPE)( \
            TYPE *, size_t, size_t, bool, bool, bool) \
            = NULL; \
    template <> \
    void XSS_EXPORT_SYMBOL qselect(TYPE *arr, \
                                   size_t k, \
                                   size_t arrsize, \
                                   bool hasnan, \
                                   bool descending, \
                                   bool trailing_nans) \
    { \
        if (internal_qselect##TYPE == NULL) { CAT(resolve_qselect, TYPE)(); } \
        (*internal_qselect##TYPE)( \
                arr, k, arrsize, hasnan, descending, trailing_nans); \
    }

#define DECLARE_INTERNAL_partial_qsort(TYPE) \
    static void CAT(resolve_partial_qsort, TYPE)(void); \
    static void (*internal_partial_qsort##TYPE)( \
            TYPE *, size_t, size_t, bool, bool, bool) \
            = NULL; \
    template <> \
    void XSS_EXPORT_SYMBOL partial_qsort(TYPE *arr, \
                                         size_t k, \
                                         size_t arrsize, \
                                         bool hasnan, \
                                         bool descending, \
                                         bool trailing_nans) \
    { \
        if (internal_partial_qsort##TYPE == NULL) { \
            CAT(resolve_partial_qsort, TYPE)(); \
        } \
        (*internal_partial_qsort##TYPE)( \
                arr, k, arrsize, hasnan, descending, trailing_nans); \
    }

#define DECLARE_INTERNAL_argsort(TYPE) \
    static void CAT(resolve_argsort, TYPE)(void); \
    static std::vector<size_t> (*internal_argsort##TYPE)( \
            const TYPE *, size_t, bool, bool, bool) \
            = NULL; \
    template <> \
    std::vector<size_t> XSS_EXPORT_SYMBOL argsort(const TYPE *arr, \
                                                  size_t arrsize, \
                                                  bool hasnan, \
                                                  bool descending, \
                                                  bool trailing_nans) \
    { \
        if (internal_argsort##TYPE == NULL) { CAT(resolve_argsort, TYPE)(); } \
        return (*internal_argsort##TYPE)( \
                arr, arrsize, hasnan, descending, trailing_nans); \
    }

#define DECLARE_INTERNAL_argselect(TYPE) \
    static void CAT(resolve_argselect, TYPE)(void); \
    static std::vector<size_t> (*internal_argselect##TYPE)( \
            const TYPE *, size_t, size_t, bool, bool, bool) \
            = NULL; \
    template <> \
    std::vector<size_t> XSS_EXPORT_SYMBOL argselect(const TYPE *arr, \
                                                    size_t k, \
                                                    size_t arrsize, \
                                                    bool hasnan, \
                                                    bool descending, \
                                                    bool trailing_nans) \
    { \
        if (internal_argselect##TYPE == NULL) { \
            CAT(resolve_argselect, TYPE)(); \
        } \
        return (*internal_argselect##TYPE)( \
                arr, k, arrsize, hasnan, descending, trailing_nans); \
    }

#else

#define DECLARE_INTERNAL_qsort(TYPE) \
    static void (*internal_qsort##TYPE)(TYPE *, size_t, bool, bool, bool) \
            = NULL; \
    template <> \
    void XSS_EXPORT_SYMBOL qsort(TYPE *arr, \
                                 size_t arrsize, \
                                 bool hasnan, \
                                 bool descending, \
                                 bool trailing_nans) \
    { \
        (*internal_qsort##TYPE)( \
                arr, arrsize, hasnan, descending, trailing_nans); \
    }

#define DECLARE_INTERNAL_qselect(TYPE) \
    static void (*internal_qselect##TYPE)( \
            TYPE *, size_t, size_t, bool, bool, bool) \
            = NULL; \
    template <> \
    void XSS_EXPORT_SYMBOL qselect(TYPE *arr, \
                                   size_t k, \
                                   size_t arrsize, \
                                   bool hasnan, \
                                   bool descending, \
                                   bool trailing_nans) \
    { \
        (*internal_qselect##TYPE)( \
                arr, k, arrsize, hasnan, descending, trailing_nans); \
    }

#define DECLARE_INTERNAL_partial_qsort(TYPE) \
    static void (*internal_partial_qsort##TYPE)( \
            TYPE *, size_t, size_t, bool, bool, bool) \
            = NULL; \
    template <> \
    void XSS_EXPORT_SYMBOL partial_qsort(TYPE *arr, \
                                         size_t k, \
                                         size_t arrsize, \
                                         bool hasnan, \
                                         bool descending, \
                                         bool trailing_nans) \
    { \
        (*internal_partial_qsort##TYPE)( \
                arr, k, arrsize, hasnan, descending, trailing_nans); \
    }

#define DECLARE_INTERNAL_argsort(TYPE) \
    static std::vector<size_t> (*internal_argsort##TYPE)( \
            const TYPE *, size_t, bool, bool, bool) \
            = NULL; \
    template <> \
    std::vector<size_t> XSS_EXPORT_SYMBOL argsort(const TYPE *arr, \
                                                  size_t arrsize, \
                                                  bool hasnan, \
                                                  bool descending, \
                                                  bool trailing_nans) \
    { \
        return (*internal_argsort##TYPE)( \
                arr, arrsize, hasnan, descending, trailing_nans); \
    }

#define DECLARE_INTERNAL_argselect(TYPE) \
    static std::vector<size_t> (*internal_argselect##TYPE)( \
            const TYPE *, size_t, size_t, bool, bool, bool) \
            = NULL; \
    template <> \
    std::vector<size_t> XSS_EXPORT_SYMBOL argselect(const TYPE *arr, \
                                                    size_t k, \
                                                    size_t arrsize, \
                                                    bool hasnan, \
                                                    bool descending, \
                                                    bool trailing_nans) \
    { \
        return (*internal_argselect##TYPE)( \
                arr, k, arrsize, hasnan, descending, trailing_nans); \
    }

#endif // _MSC_VER

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

#define DISPATCH_KV_FUNC(func, TYPE1, TYPE2, ISA) \
    static XSS_ATTRIBUTE_CONSTRUCTOR void CAT( \
            CAT(CAT(CAT(resolve_, func), _), TYPE1), TYPE2)(void) \
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

#ifdef _MSC_VER
#define DECLARE_ALL_KEYVALUE_METHODS(TYPE1, TYPE2) \
    static void CAT(CAT(resolve_keyvalue_select_, TYPE1), TYPE2)(void); \
    static void CAT(CAT(resolve_keyvalue_partial_sort_, TYPE1), TYPE2)(void); \
    static void CAT(CAT(resolve_keyvalue_qsort_, TYPE1), TYPE2)(void); \
    static void(CAT(CAT(*internal_keyvalue_qsort_, TYPE1), TYPE2))( \
            TYPE1 *, TYPE2 *, size_t, bool, bool) \
            = NULL; \
    static void(CAT(CAT(*internal_keyvalue_select_, TYPE1), TYPE2))( \
            TYPE1 *, TYPE2 *, size_t, size_t, bool, bool) \
            = NULL; \
    static void(CAT(CAT(*internal_keyvalue_partial_sort_, TYPE1), TYPE2))( \
            TYPE1 *, TYPE2 *, size_t, size_t, bool, bool) \
            = NULL; \
    template <> \
    void XSS_EXPORT_SYMBOL keyvalue_qsort(TYPE1 *key, \
                                          TYPE2 *val, \
                                          size_t arrsize, \
                                          bool hasnan, \
                                          bool descending) \
    { \
        if ((CAT(CAT(*internal_keyvalue_qsort_, TYPE1), TYPE2)) == NULL) { \
            CAT(CAT(resolve_keyvalue_qsort_, TYPE1), TYPE2)(); \
        } \
        (CAT(CAT(*internal_keyvalue_qsort_, TYPE1), TYPE2))( \
                key, val, arrsize, hasnan, descending); \
    } \
    template <> \
    void XSS_EXPORT_SYMBOL keyvalue_select(TYPE1 *key, \
                                           TYPE2 *val, \
                                           size_t k, \
                                           size_t arrsize, \
                                           bool hasnan, \
                                           bool descending) \
    { \
        if ((CAT(CAT(*internal_keyvalue_select_, TYPE1), TYPE2)) == NULL) { \
            CAT(CAT(resolve_keyvalue_select_, TYPE1), TYPE2)(); \
        } \
        (CAT(CAT(*internal_keyvalue_select_, TYPE1), TYPE2))( \
                key, val, k, arrsize, hasnan, descending); \
    } \
    template <> \
    void XSS_EXPORT_SYMBOL keyvalue_partial_sort(TYPE1 *key, \
                                                 TYPE2 *val, \
                                                 size_t k, \
                                                 size_t arrsize, \
                                                 bool hasnan, \
                                                 bool descending) \
    { \
        if ((CAT(CAT(*internal_keyvalue_partial_sort_, TYPE1), TYPE2)) \
            == NULL) { \
            CAT(CAT(resolve_keyvalue_partial_sort_, TYPE1), TYPE2)(); \
        } \
        (CAT(CAT(*internal_keyvalue_partial_sort_, TYPE1), TYPE2))( \
                key, val, k, arrsize, hasnan, descending); \
    }
#else
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
    template <> \
    void XSS_EXPORT_SYMBOL keyvalue_qsort(TYPE1 *key, \
                                          TYPE2 *val, \
                                          size_t arrsize, \
                                          bool hasnan, \
                                          bool descending) \
    { \
        (CAT(CAT(*internal_keyvalue_qsort_, TYPE1), TYPE2))( \
                key, val, arrsize, hasnan, descending); \
    } \
    template <> \
    void XSS_EXPORT_SYMBOL keyvalue_select(TYPE1 *key, \
                                           TYPE2 *val, \
                                           size_t k, \
                                           size_t arrsize, \
                                           bool hasnan, \
                                           bool descending) \
    { \
        (CAT(CAT(*internal_keyvalue_select_, TYPE1), TYPE2))( \
                key, val, k, arrsize, hasnan, descending); \
    } \
    template <> \
    void XSS_EXPORT_SYMBOL keyvalue_partial_sort(TYPE1 *key, \
                                                 TYPE2 *val, \
                                                 size_t k, \
                                                 size_t arrsize, \
                                                 bool hasnan, \
                                                 bool descending) \
    { \
        (CAT(CAT(*internal_keyvalue_partial_sort_, TYPE1), TYPE2))( \
                key, val, k, arrsize, hasnan, descending); \
    }
#endif // _MSC_VER

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
