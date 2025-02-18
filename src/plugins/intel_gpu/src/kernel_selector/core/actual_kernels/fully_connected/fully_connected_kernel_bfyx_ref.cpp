﻿// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <vector>

#include "fully_connected_kernel_bfyx_ref.h"
#include "kernel_selector_utils.h"

namespace kernel_selector {
ParamsKey FullyConnected_bfyx_Ref::GetSupportedKey() const {
    ParamsKey k;
    k.EnableInputDataType(Datatype::F16);
    k.EnableInputDataType(Datatype::F32);
    k.EnableInputDataType(Datatype::INT8);
    k.EnableInputDataType(Datatype::UINT8);
    k.EnableOutputDataType(Datatype::F16);
    k.EnableOutputDataType(Datatype::F32);
    k.EnableOutputDataType(Datatype::INT8);
    k.EnableOutputDataType(Datatype::UINT8);
    k.EnableInputWeightsType(WeightsType::F16);
    k.EnableInputWeightsType(WeightsType::F32);
    k.EnableInputWeightsType(WeightsType::INT8);
    k.EnableAllInputLayout();
    k.EnableDifferentInputWeightsTypes();
    k.EnableDifferentTypes();
    k.EnableInputLayout(DataLayout::bf);
    k.EnableInputLayout(DataLayout::bfyx);
    k.EnableOutputLayout(DataLayout::bf);
    k.EnableOutputLayout(DataLayout::fb);
    k.EnableOutputLayout(DataLayout::bfyx);
    k.EnableBiasPerOutput();
    k.EnableBiasPerFeature();
    k.EnableNonBiasTerm();
    k.EnableTensorOffset();
    k.EnableTensorPitches();
    k.EnableBatching();
    k.EnableQuantization(QuantizationType::SYMMETRIC);
    return k;
}

FullyConnected_bfyx_Ref::DispatchData FullyConnected_bfyx_Ref::SetDefault(const fully_connected_params& params,
                                                                          int) const {
    auto dispatchData = Parent::SetDefault(params);

    std::vector<size_t> global = {params.output.Feature().v, params.output.Batch().v, 1};
    if (params.output.GetLayout() == DataLayout::bfyx)
        global = {params.output.Feature().v * params.output.Y().v, params.output.Batch().v, 1};

    dispatchData.gws = global;
    dispatchData.lws = GetOptimalLocalWorkGroupSizes(dispatchData.gws, params.engineInfo);

    return dispatchData;
}

KernelsPriority FullyConnected_bfyx_Ref::GetKernelsPriority(const Params& /*params*/, const optional_params& /*options*/) const {
    return DONT_USE_IF_HAVE_SOMETHING_ELSE;
}

JitConstants FullyConnected_bfyx_Ref::GetJitConstants(const fully_connected_params& params,
    const FullyConnectedKernelBase::DispatchData& dispatchData) const {
    JitConstants jit = Parent::GetJitConstants(params, dispatchData);
    Datatype accumulator_dt;
    Datatype activation_dt;

    if (params.quantization != QuantizationType::NONE) {
        accumulator_dt = Datatype::INT32;
        activation_dt = Datatype::F32;
    } else {
        accumulator_dt = Datatype::F32;
        activation_dt = Datatype::F32;
    }
    if (params.output.GetLayout() == DataLayout::bfyx)
        jit.AddConstant(MakeJitConstant("OUTPUT_3D", true));
    jit.Merge(MakeTypeJitConstants(activation_dt, "ACTIVATION"));
    jit.Merge(MakeTypeJitConstants(accumulator_dt, "ACCUMULATOR"));
    jit.Merge(MakeActivationJitConstants(params.activations, activation_dt, "_TYPED"));

    if (!params.fused_ops.empty()) {
        FusedOpsConfiguration conf = { "", {"b", "ofm", "oym", "0"}, "dequantized", activation_dt, 1 };
        jit.Merge(MakeFusedOpsJitConstants(params, { conf }));
    }
    return jit;
}

KernelsData FullyConnected_bfyx_Ref::GetKernelsData(const Params& params, const optional_params& options) const {
    KernelsData res = {};
    for (size_t i = 0; i < autoTuneOptions.size(); i++) {
        KernelsData kd = GetTunedKernelsDataByIndex(
            params,
            options,
            DataLayout::bfyx,
            WeightsLayout::oiyx,
            static_cast<int>(i));
        if (!kd.empty()) {
            res.emplace_back(kd[0]);
        }
    }

    return res;
}

bool FullyConnected_bfyx_Ref::Validate(const Params& params, const optional_params& options) const {
    if (!Parent::Validate(params, options))
        return false;

    // int8 validation
    const auto& fc_params = static_cast<const fully_connected_params&>(params);
    auto input_type = fc_params.inputs[0].GetDType();
    auto output_type = fc_params.output.GetDType();

    // int8/uint8 inputs (quantization case) require additional checks
    // require some additional checks.
    if ((input_type != Datatype::UINT8 && input_type != Datatype::INT8) &&
        (output_type != Datatype::UINT8 && output_type != Datatype::INT8))
        return true;

    bool is_quantization = (input_type == Datatype::INT8 || input_type == Datatype::UINT8) &&
                           (output_type == Datatype::INT8 || output_type == Datatype::UINT8 ||
                            output_type == Datatype::F32 || output_type == Datatype::F16) &&
                           (fc_params.weights.GetDType() == WeightsType::INT8);

    bool has_fused_op = (input_type == Datatype::F32 || input_type == Datatype::F16) &&
                        !fc_params.fused_ops.empty() &&
                        (output_type == Datatype::INT8 || output_type == Datatype::UINT8);

    if (!is_quantization && !has_fused_op)
        return false;

    // We don't support 4d output
    if (fc_params.output.GetLayout() == DataLayout::bfyx && fc_params.output.X().v > 1)
        return false;

    return true;
}

}  // namespace kernel_selector
