// SPR specific routines:
#include "x86simdsort-static-incl.h"
#include "x86simdsort-internal.h"

namespace xss {
namespace fp16_spr {
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
} // namespace fp16_spr
} // namespace xss
