/*
    enoki/python.h -- pybind11 type casters for static and dynamic arrays

    Enoki is a C++ template library that enables transparent vectorization
    of numerical kernels using SIMD instruction sets available on current
    processor architectures.

    Copyright (c) 2017 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a BSD-style
    license that can be found in the LICENSE.txt file.
*/

#pragma once

#include "array.h"
#include <pybind11/numpy.h>

NAMESPACE_BEGIN(pybind11)
NAMESPACE_BEGIN(detail)

template <typename T, typename = void> struct array_shape_descr {
    static PYBIND11_DESCR name() { return _(""); }
    static PYBIND11_DESCR name_cont() { return _(""); }
};

template <typename T> struct array_shape_descr<T, std::enable_if_t<enoki::is_sarray<T>::value>> {
    static PYBIND11_DESCR name() {
        return _<T::Size>() + array_shape_descr<typename T::Scalar>::name_cont();
    }
    static PYBIND11_DESCR name_cont() {
        return _(", ") + _<T::Size>() + array_shape_descr<typename T::Scalar>::name_cont();
    }
};

template <typename T> struct array_shape_descr<T, std::enable_if_t<enoki::is_darray<T>::value>> {
    static PYBIND11_DESCR name() {
        return _("n") + array_shape_descr<typename T::Scalar>::name_cont();
    }
    static PYBIND11_DESCR name_cont() {
        return _(", n") + array_shape_descr<typename T::Scalar>::name_cont();
    }
};

template<typename Type> struct type_caster<Type, std::enable_if_t<enoki::is_array<Type>::value>> {
    typedef typename Type::Scalar     Scalar;
    typedef typename Type::BaseScalar BaseScalar;

    bool load(handle src, bool) {
        auto arr = array_t<BaseScalar, array::c_style | array::forcecast>::ensure(src);
        if (!arr)
            return false;

        constexpr size_t ndim = enoki::array_depth<Type>::value;
        if (ndim != arr.ndim())
            return false;

        std::array<size_t, ndim> shape;
        std::copy_n(arr.shape(), ndim, shape.begin());

        enoki::resize(value, shape);
        const BaseScalar *buf = static_cast<const BaseScalar *>(arr.data());
        read_buffer(buf, value);

        return true;
    }

    static handle cast(const Type *src, return_value_policy policy, handle parent) {
        return cast(*src, policy, parent);
    }

    static handle cast(const Type &src, return_value_policy /* policy */, handle /* parent */) {
        if (enoki::ragged(src))
            throw type_error("Ragged arrays are not supported!");

        auto shape = enoki::shape(src);
        decltype(shape) stride;

        stride[shape.size() - 1] = sizeof(BaseScalar);
        for (int i = (int) shape.size() - 2; i >= 0; --i)
            stride[i] = shape[i + 1] * stride[i + 1];

        buffer_info info(nullptr, sizeof(BaseScalar),
                         format_descriptor<BaseScalar>::value, shape.size(),
                         std::vector<size_t>(shape.begin(), shape.end()),
                         std::vector<size_t>(stride.begin(), stride.end()));

        array arr(info);
        BaseScalar *buf = static_cast<BaseScalar *>(arr.mutable_data());
        write_buffer(buf, src);
        return arr.release();
    }

    template <typename _T> using cast_op_type = pybind11::detail::cast_op_type<_T>;

    static PYBIND11_DESCR name() {
        return pybind11::detail::type_descr(
            _("numpy.ndarray[dtype=") +
            npy_format_descriptor<BaseScalar>::name() + _(", shape=(") +
            array_shape_descr<Type>::name() + _(")]"));
    }

    operator Type*() { return &value; }
    operator Type&() { return value; }

private:
    template <typename T, std::enable_if_t<!enoki::is_array<T>::value, int> = 0>
    ENOKI_INLINE static void write_buffer(BaseScalar *&, const T &) { }

    template <typename T, std::enable_if_t<enoki::is_array<T>::value, int> = 0>
    ENOKI_INLINE static void write_buffer(BaseScalar *&buf, const T &value_) {
        const auto &value = value_.derived();
        size_t size = value.size();

        if (std::is_arithmetic<typename T::Scalar>::value) {
            memcpy(buf, &value.coeff(0), sizeof(typename T::Scalar) * size);
            buf += size;
        } else {
            for (size_t i = 0; i < size; ++i)
                write_buffer(buf, value.coeff(i));
        }
    }

    template <typename T, std::enable_if_t<!enoki::is_array<T>::value, int> = 0>
    ENOKI_INLINE static void read_buffer(const BaseScalar *&, T &) { }

    template <typename T, std::enable_if_t<enoki::is_array<T>::value, int> = 0>
    ENOKI_INLINE static void read_buffer(const BaseScalar *&buf, T &value_) {
        auto &value = value_.derived();
        size_t size = value.size();

        if (std::is_arithmetic<typename T::Scalar>::value) {
            memcpy(&value.coeff(0), buf, sizeof(typename T::Scalar) * size);
            buf += size;
        } else {
            for (size_t i = 0; i < size; ++i)
                read_buffer(buf, value.coeff(i));
        }
    }

private:
    Type value;
};

NAMESPACE_END(detail)
NAMESPACE_END(pybind11)
