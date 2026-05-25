#include "custom-compare.h"
#include <algorithm>
#include <numeric>

namespace xss {
namespace utils {
    /*
     * O(1) permute array in place: stolen from
     * http://www.davidespataro.it/apply-a-permutation-to-a-vector
     */
    template <typename T>
    void apply_permutation_in_place(T *arr, std::vector<size_t> arg)
    {
        for (size_t i = 0; i < arg.size(); i++) {
            size_t curr = i;
            size_t next = arg[curr];
            while (next != i) {
                std::swap(arr[curr], arr[next]);
                arg[curr] = curr;
                curr = next;
                next = arg[next];
            }
            arg[curr] = curr;
        }
    }
    template <typename T>
    decltype(auto)
    get_cmp_func(bool hasnan, bool reverse, bool trailing_nans = true)
    {
        std::function<bool(T, T)> cmp;
        if (hasnan) {
            if (trailing_nans) {
                if (reverse == true) {
                    cmp = compare_nan_end<T, std::greater<T>>();
                }
                else {
                    cmp = compare<T, std::less<T>>();
                }
            }
            else {
                if (reverse == true) { cmp = compare<T, std::greater<T>>(); }
                else {
                    cmp = compare_nan_begin<T, std::less<T>>();
                }
            }
        }
        else {
            if (reverse == true) { cmp = std::greater<T>(); }
            else {
                cmp = std::less<T>();
            }
        }
        return cmp;
    }
} // namespace utils

namespace scalar {
    template <typename T>
    void qsort(T *arr,
               size_t arrsize,
               bool hasnan,
               bool reversed,
               bool trailing_nans)
    {
        std::sort(arr,
                  arr + arrsize,
                  xss::utils::get_cmp_func<T>(hasnan, reversed, trailing_nans));
    }

    template <typename T>
    void qselect(T *arr,
                 size_t k,
                 size_t arrsize,
                 bool hasnan,
                 bool reversed,
                 bool trailing_nans)
    {
        std::nth_element(
                arr,
                arr + k,
                arr + arrsize,
                xss::utils::get_cmp_func<T>(hasnan, reversed, trailing_nans));
    }
    template <typename T>
    void partial_qsort(T *arr,
                       size_t k,
                       size_t arrsize,
                       bool hasnan,
                       bool reversed,
                       bool trailing_nans)
    {
        std::partial_sort(
                arr,
                arr + k,
                arr + arrsize,
                xss::utils::get_cmp_func<T>(hasnan, reversed, trailing_nans));
    }
    template <typename T>
    std::vector<size_t> argsort(const T *arr,
                                size_t arrsize,
                                bool hasnan,
                                bool reversed,
                                bool trailing_nans)
    {
        UNUSED(hasnan);
        std::vector<size_t> arg(arrsize);
        std::iota(arg.begin(), arg.end(), 0);
        if (trailing_nans) {
            if (reversed) {
                std::sort(arg.begin(),
                          arg.end(),
                          compare_arg_nan_end<T, std::greater<T>>(arr));
            }
            else {
                std::sort(arg.begin(),
                          arg.end(),
                          compare_arg<T, std::less<T>>(arr));
            }
        }
        else {
            if (reversed) {
                std::sort(arg.begin(),
                          arg.end(),
                          compare_arg<T, std::greater<T>>(arr));
            }
            else {
                std::sort(arg.begin(),
                          arg.end(),
                          compare_arg_nan_begin<T, std::less<T>>(arr));
            }
        }
        return arg;
    }
    template <typename T>
    std::vector<size_t> argselect(const T *arr,
                                  size_t k,
                                  size_t arrsize,
                                  bool hasnan,
                                  bool trailing_nans)
    {
        UNUSED(hasnan);
        std::vector<size_t> arg(arrsize);
        std::iota(arg.begin(), arg.end(), 0);
        if (hasnan && !trailing_nans) {
            std::nth_element(arg.begin(),
                             arg.begin() + k,
                             arg.end(),
                             compare_arg_nan_begin<T, std::less<T>>(arr));
        }
        else {
            std::nth_element(arg.begin(),
                             arg.begin() + k,
                             arg.end(),
                             compare_arg<T, std::less<T>>(arr));
        }
        return arg;
    }
    template <typename T1, typename T2>
    void keyvalue_qsort(
            T1 *key, T2 *val, size_t arrsize, bool hasnan, bool descending)
    {
        std::vector<size_t> arg
                = argsort(key, arrsize, hasnan, descending, true);
        utils::apply_permutation_in_place(key, arg);
        utils::apply_permutation_in_place(val, arg);
    }
    template <typename T1, typename T2>
    void keyvalue_select(T1 *key,
                         T2 *val,
                         size_t k,
                         size_t arrsize,
                         bool hasnan,
                         bool descending)
    {
        // Note that this does a full kv-sort
        UNUSED(k);
        keyvalue_qsort(key, val, arrsize, hasnan, descending);
    }
    template <typename T1, typename T2>
    void keyvalue_partial_sort(T1 *key,
                               T2 *val,
                               size_t k,
                               size_t arrsize,
                               bool hasnan,
                               bool descending)
    {
        // Note that this does a full kv-sort
        UNUSED(k);
        keyvalue_qsort(key, val, arrsize, hasnan, descending);
    }

} // namespace scalar
} // namespace xss
