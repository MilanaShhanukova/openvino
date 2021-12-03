// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <mkldnn_node.h>
#include <mkldnn.hpp>
#include <cassert>

#include <cpu/ref_eltwise.hpp>
#include <cpu/ref_depthwise_injector.hpp>
#include "utils/bfloat16.hpp"
#include "utils/cpu_utils.hpp"
#include "ie_parallel.hpp"

using namespace InferenceEngine;

namespace MKLDNNPlugin {

struct jit_normalize_config_params {
    bool is_nchw;
    bool is_nhwc;
    bool is_blk;
    bool across_spatial;
    mkldnn::memory::data_type src_dt;
    mkldnn::memory::data_type dst_dt;
    int src_data_size;
    int dst_data_size;
    size_t n, c, h, w;
};

struct jit_normalize_call_args {
    const void *src;
    void *dst;
    const float *modulo;
    const float *fused_factor;
    size_t src_stride;
    size_t dst_stride;
    size_t work_amount;
    size_t oc_off;
};

struct jit_uni_normalize_modulo_kernel {
    void (*ker_)(const jit_normalize_call_args *);

    void operator()(const jit_normalize_call_args *args) {
        assert(ker_);
        ker_(args);
    }

    jit_uni_normalize_modulo_kernel(jit_normalize_config_params jcp) : ker_(nullptr), jcp_(jcp) {}
    virtual ~jit_uni_normalize_modulo_kernel() {}

    virtual void create_ker() = 0;

    jit_normalize_config_params jcp_;
};

struct jit_uni_normalize_kernel {
    void (*ker_)(const jit_normalize_call_args *);

    void operator()(const jit_normalize_call_args *args) {
        assert(ker_);
        ker_(args);
    }

    explicit jit_uni_normalize_kernel(jit_normalize_config_params jcp, const mkldnn_primitive_attr &attr) : ker_(nullptr), jcp_(jcp), attr_(attr) {}
    virtual ~jit_uni_normalize_kernel() {}

    virtual void create_ker() = 0;

    jit_normalize_config_params jcp_;
    const mkldnn_primitive_attr &attr_;
};

class MKLDNNNormalizeL2Node : public MKLDNNNode {
public:
    MKLDNNNormalizeL2Node(const std::shared_ptr<ngraph::Node>& op, const mkldnn::engine& eng, MKLDNNWeightsSharing::Ptr &cache);

    static bool isSupportedOperation(const std::shared_ptr<const ngraph::Node>& op, std::string& errorMessage) noexcept;
    void getSupportedDescriptors() override {};
    void initSupportedPrimitiveDescriptors() override;
    void createPrimitive() override;
    bool created() const override;
    void execute(mkldnn::stream strm) override;
    bool canBeInPlace() const override {
        return false;
    }
    bool canFuse(const MKLDNNNodePtr& node) const override;

    std::vector<VectorDims> shapeInfer() const override;
    void prepareParams() override;
    void executeDynamicImpl(mkldnn::stream strm) override { execute(strm); }

private:
    enum class NormEpsMode {
        ADD,
        MAX
    };

    struct NormalizeL2Attrs {
        NormEpsMode epsMode = NormEpsMode::ADD;
        bool across_spatial = true;
        bool cornerCase = false;
        float eps = 1e-10f;

        bool is_nchw = false;
        bool is_nhwc = false;
        bool is_blk = false;

        InferenceEngine::Precision input_prec = Precision::UNSPECIFIED;
        InferenceEngine::Precision output_prec = Precision::UNSPECIFIED;
        size_t src_data_size = 0lu;
        size_t dst_data_size = 0lu;
    } attrs;

    class NormalizeL2Executor {
    public:
        NormalizeL2Executor() = default;
        virtual void exec(const uint8_t *src_ptr, uint8_t *dst_ptr) = 0;
        virtual ~NormalizeL2Executor() = default;

        static std::shared_ptr<NormalizeL2Executor> getNormalizeL2Executor(const NormalizeL2Attrs& attrs,
                                                                           const mkldnn::primitive_attr& kernel_attr,
                                                                           const VectorDims& dims);

    protected:
        inline float epsApply(const float &modulo, const NormEpsMode mode, const float eps) const {
            return mode == NormEpsMode::ADD ? modulo + eps : std::max(modulo, eps);
        }

    private:
        template <typename in_data_t, typename out_data_t>
        static std::shared_ptr<NormalizeL2Executor> makeExecutor(const NormalizeL2Attrs& attrs,
                                                                 const mkldnn::primitive_attr& kernel_attrs,
                                                                 const VectorDims& dims);

        struct NormalizeContext {
            std::shared_ptr<NormalizeL2Executor> executor;
            NormalizeL2Attrs attrs;
            mkldnn::primitive_attr kernel_attrs;
            VectorDims dims;
        };

        template<typename T>
        struct NormalizeExecutorCreation {
            using src_t = typename std::tuple_element<0, T>::type;
            using dst_t = typename std::tuple_element<1, T>::type;

            void operator()(NormalizeContext& ctx) {
                ctx.executor = NormalizeL2Executor::makeExecutor<src_t, dst_t>(ctx.attrs, ctx.kernel_attrs, ctx.dims);
            }
        };
    };

    template <typename in_data_t, typename out_data_t> struct NormalizeL2CornerCaseExecutor;
    template <typename in_data_t, typename out_data_t> struct NormalizeL2JitExecutor;
    template <typename in_data_t, typename out_data_t> struct NormalizeL2ReferenceExecutor;

    mkldnn::primitive_attr kernel_attrs;

    void setPostOps(mkldnn::primitive_attr& kernel_attrs, const VectorDims& dims, bool initWeights = false);

    static constexpr size_t DATA = 0;
    static constexpr size_t AXES = 1;

    using executorPtr = std::shared_ptr<NormalizeL2Executor>;
    executorPtr execPtr = nullptr;
};

}  // namespace MKLDNNPlugin

