﻿// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "fully_connected_kernel_bf_io_input_spatial.h"

namespace kernel_selector {
ParamsKey FullyConnected_bf_io_input_spatial::GetSupportedKey() const {
    ParamsKey k;
    k.EnableInputDataType(Datatype::F16);
    k.EnableInputDataType(Datatype::F32);
    k.EnableOutputDataType(Datatype::F16);
    k.EnableOutputDataType(Datatype::F32);
    k.EnableInputWeightsType(WeightsType::F16);
    k.EnableInputWeightsType(WeightsType::F32);
    k.EnableAllInputLayout();
    k.EnableOutputLayout(DataLayout::bf);
    k.EnableBatching();
    k.EnableBiasPerFeature();
    k.EnableNonBiasTerm();
    return k;
}

FullyConnected_bf_io_input_spatial::DispatchData FullyConnected_bf_io_input_spatial::SetDefault(
    const fully_connected_params& arg,
    int) const {
    auto dispatchData = FullyConnectedKernelBase::SetDefault(arg);

    dispatchData.gws[0] = Align(arg.output.LogicalSize() / arg.inputs[0].Batch().v, 16);
    dispatchData.gws[1] = arg.inputs[0].Batch().v;
    dispatchData.gws[2] = 1;

    dispatchData.lws[0] = 16;
    dispatchData.lws[1] = 1;
    dispatchData.lws[2] = 1;

    return dispatchData;
}

KernelsPriority FullyConnected_bf_io_input_spatial::GetKernelsPriority(const Params& params, const optional_params& /*options*/) const {
    const auto& p = static_cast<const fully_connected_params&>(params);
    const auto& input = p.inputs[0];
    const auto& output = p.output;

    if (input.Batch().v == 1 && output.Batch().v == 1)
        if ((input.LogicalSize() / output.Batch().v >= 4096) && (output.Feature().v >= 4096))
            return FORCE_PRIORITY_1;

    return DONT_USE_IF_HAVE_SOMETHING_ELSE;
}

bool FullyConnected_bf_io_input_spatial::Validate(const Params& p, const optional_params& o) const {
    if (!FullyConnectedKernelBase::Validate(p, o)) {
        return false;
    }

    const auto& params = static_cast<const fully_connected_params&>(p);

    const auto& input = params.inputs[0];
    const auto& output = params.output;
    if ((input.GetLayout() != DataLayout::bfyx && input.GetLayout() != DataLayout::bf) ||
        (output.GetLayout() != DataLayout::bfyx && output.GetLayout() != DataLayout::bf)) {
        return false;
    }

    return true;
}

KernelsData FullyConnected_bf_io_input_spatial::GetKernelsData(const Params& params,
                                                               const optional_params& optParams) const {
    return GetCommonKernelsData(params, optParams, DataLayout::bf, WeightsLayout::io);
}

KernelsData FullyConnected_bf_io_input_spatial::GetKernelsDataForAutoTune(const Params& params, const optional_params& optParams) const {
    KernelsData res = {};

    for (size_t i = 0; i < autoTuneOptions.size(); i++) {
        KernelsData kd = GetTunedKernelsDataByIndex(params,
                                                    optParams,
                                                    DataLayout::bf,
                                                    WeightsLayout::io,
                                                    static_cast<int>(i));
        if (!kd.empty()) {
            res.emplace_back(kd[0]);
        }
    }

    return res;
}
}  // namespace kernel_selector
