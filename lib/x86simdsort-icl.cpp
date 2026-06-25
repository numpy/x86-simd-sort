// ICL specific routines:
#include "x86simdsort-static-incl.h"
#include "x86simdsort-internal.h"
#ifdef _MSC_VER
#include "avx512-16bit-qsort.hpp"
#endif

namespace xss {
namespace avx512 {
    template <>
    void qsort(uint16_t *arr,
               size_t size,
               bool hasnan,
               bool descending,
               bool nans_last)
    {
        x86simdsortStatic::qsort(arr, size, hasnan, descending, nans_last);
    }
    template <>
    void qselect(uint16_t *arr,
                 size_t k,
                 size_t arrsize,
                 bool hasnan,
                 bool descending,
                 bool nans_last)
    {
        x86simdsortStatic::qselect(
                arr, k, arrsize, hasnan, descending, nans_last);
    }
    template <>
    void partial_qsort(uint16_t *arr,
                       size_t k,
                       size_t arrsize,
                       bool hasnan,
                       bool descending,
                       bool nans_last)
    {
        x86simdsortStatic::partial_qsort(
                arr, k, arrsize, hasnan, descending, nans_last);
    }
    template <>
    void qsort(int16_t *arr,
               size_t size,
               bool hasnan,
               bool descending,
               bool nans_last)
    {
        x86simdsortStatic::qsort(arr, size, hasnan, descending, nans_last);
    }
    template <>
    void qselect(int16_t *arr,
                 size_t k,
                 size_t arrsize,
                 bool hasnan,
                 bool descending,
                 bool nans_last)
    {
        x86simdsortStatic::qselect(
                arr, k, arrsize, hasnan, descending, nans_last);
    }
    template <>
    void partial_qsort(int16_t *arr,
                       size_t k,
                       size_t arrsize,
                       bool hasnan,
                       bool descending,
                       bool nans_last)
    {
        x86simdsortStatic::partial_qsort(
                arr, k, arrsize, hasnan, descending, nans_last);
    }
} // namespace avx512
namespace fp16_icl {
#ifdef __FLT16_MAX__
    template <>
    void qsort(_Float16 *arr,
               size_t size,
               bool hasnan,
               bool descending,
               bool nans_last)
    {
        x86simdsortStatic::qsort(arr, size, hasnan, descending, nans_last);
    }
    template <>
    void qselect(_Float16 *arr,
                 size_t k,
                 size_t arrsize,
                 bool hasnan,
                 bool descending,
                 bool nans_last)
    {
        x86simdsortStatic::qselect(
                arr, k, arrsize, hasnan, descending, nans_last);
    }
    template <>
    void partial_qsort(_Float16 *arr,
                       size_t k,
                       size_t arrsize,
                       bool hasnan,
                       bool descending,
                       bool nans_last)
    {
        x86simdsortStatic::partial_qsort(
                arr, k, arrsize, hasnan, descending, nans_last);
    }
#endif
} // namespace fp16_icl
} // namespace xss
