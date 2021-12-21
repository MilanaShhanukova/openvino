// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "openvino/frontend/exception.hpp"
#include "openvino/opsets/opset6.hpp"

namespace ov {
namespace frontend {
namespace pdpd {
#ifdef _WIN32
const char PATH_SEPARATOR = '\\';
#    if defined(OPENVINO_ENABLE_UNICODE_PATH_SUPPORT)
const wchar_t WPATH_SEPARATOR = L'\\';
#    endif
#else
const char PATH_SEPARATOR = '/';
#endif

template <typename T>
inline std::basic_string<T> get_path_sep() {
    return std::basic_string<T>{PATH_SEPARATOR};
}

#if defined(OPENVINO_ENABLE_UNICODE_PATH_SUPPORT) && defined(_WIN32)
template <>
inline std::basic_string<wchar_t> get_path_sep() {
    return std::basic_string<wchar_t>{WPATH_SEPARATOR};
}
#endif

template <typename T>
bool endsWith(const std::basic_string<T>& str, const std::basic_string<T>& suffix) {
    if (str.length() >= suffix.length()) {
        return (0 == str.compare(str.length() - suffix.length(), suffix.length(), suffix));
    }
    return false;
}

std::shared_ptr<Node> reorder_axes(const Output<Node>& value, std::vector<size_t> axes_order);
}  // namespace pdpd
}  // namespace frontend
}  // namespace ov
