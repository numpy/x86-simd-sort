#ifndef UTILS_CUSTOM_COMPARE
#define UTILS_CUSTOM_COMPARE

#include <limits>
#include <cmath>
#include "xss-custom-float.h"

/*
 * Custom comparator class to handle NAN's: treats NAN  > INF
 */
template <typename T, typename Comparator>
struct compare {
    static constexpr auto op = Comparator {};
    bool operator()(const T a, const T b)
    {
        if constexpr (xss::fp::is_floating_point_v<T>) {
            T inf = xss::fp::infinity<T>();
            T one = (T)1.0;
            if (!xss::fp::isunordered(a, b)) { return op(a, b); }
            else if ((xss::fp::isnan(a)) && (!xss::fp::isnan(b))) {
                return b == inf ? op(inf, one) : op(inf, b);
            }
            else if ((!xss::fp::isnan(a)) && (xss::fp::isnan(b))) {
                return a == inf ? op(one, inf) : op(a, inf);
            }
            else {
                return op(one, one);
            }
        }
        else {
            return op(a, b);
        }
    }
};

template <typename T, typename Comparator>
struct compare_arg {
    compare_arg(const T *arr)
    {
        this->arr = arr;
    }
    bool operator()(const int64_t a, const int64_t b)
    {
        return compare<T, Comparator>()(arr[a], arr[b]);
    }
    const T *arr;
};

/*
 * Comparator that always places NaN at the end of the sorted array,
 * regardless of whether Comparator is ascending or descending.
 */
template <typename T, typename Comparator>
struct compare_nan_end {
    static constexpr auto op = Comparator {};
    bool operator()(const T a, const T b) const
    {
        if constexpr (xss::fp::is_floating_point_v<T>) {
            bool a_nan = xss::fp::isnan(a);
            bool b_nan = xss::fp::isnan(b);
            if (!a_nan && !b_nan) { return op(a, b); }
            if (a_nan && b_nan) { return false; }
            return !a_nan; // b is NaN → a before b → NaN at end
        }
        else {
            return op(a, b);
        }
    }
};

/*
 * Comparator that always places NaN at the beginning of the sorted array,
 * regardless of whether Comparator is ascending or descending.
 */
template <typename T, typename Comparator>
struct compare_nan_begin {
    static constexpr auto op = Comparator {};
    bool operator()(const T a, const T b) const
    {
        if constexpr (xss::fp::is_floating_point_v<T>) {
            bool a_nan = xss::fp::isnan(a);
            bool b_nan = xss::fp::isnan(b);
            if (!a_nan && !b_nan) { return op(a, b); }
            if (a_nan && b_nan) { return false; }
            return a_nan; // a is NaN → a before b → NaN at beginning
        }
        else {
            return op(a, b);
        }
    }
};

template <typename T, typename Comparator>
struct compare_arg_nan_end {
    compare_arg_nan_end(const T *arr) : arr(arr) {}
    bool operator()(const int64_t a, const int64_t b) const
    {
        return compare_nan_end<T, Comparator>()(arr[a], arr[b]);
    }
    const T *arr;
};

template <typename T, typename Comparator>
struct compare_arg_nan_begin {
    compare_arg_nan_begin(const T *arr) : arr(arr) {}
    bool operator()(const int64_t a, const int64_t b) const
    {
        return compare_nan_begin<T, Comparator>()(arr[a], arr[b]);
    }
    const T *arr;
};

#endif // UTILS_CUSTOM_COMPARE