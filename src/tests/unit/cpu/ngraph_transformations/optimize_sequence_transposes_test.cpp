// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <gtest/gtest.h>

#include <string>
#include <memory>

#include <ngraph/function.hpp>
#include <ngraph/opsets/opset1.hpp>
#include <ngraph/opsets/opset5.hpp>
#include <ngraph_transformations/rnn_sequences_optimization.hpp>
#include <transformations/init_node_info.hpp>
#include <transformations/utils/utils.hpp>
#include <ngraph_ops/type_relaxed.hpp>
#include <ngraph/pass/manager.hpp>
#include "common_test_utils/ngraph_test_utils.hpp"

using namespace testing;
using namespace MKLDNNPlugin;

TEST(TransformationTests, OptimizeLSTMSequenceTransposesTest) {
    std::shared_ptr<ngraph::Function> f(nullptr), f_ref(nullptr);
    {
        auto X = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::Shape{ 2, 1, 16 });
        auto Y = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::Shape{ 1, 1, 128 });
        auto Z = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::Shape{ 1, 1, 128 });

        auto w_val = std::vector<float>(512 * 16, 0);
        auto r_val = std::vector<float>(512 * 128, 0);
        auto b_val = std::vector<float>(512, 0);
        auto W = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 512, 16 }, w_val);
        auto R = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 512, 128 }, r_val);
        auto B = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 512 }, b_val);

        auto transpose_before_const = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 3 }, { 1, 0, 2 });
        auto transpose_before = std::make_shared<ngraph::opset1::Transpose>(X, transpose_before_const);

        auto seq_lengths = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 1 }, { 2 });
        auto lstm_seq = std::make_shared<ngraph::opset5::LSTMSequence>(transpose_before, Y, Z, seq_lengths, W, R, B, 128,
            ngraph::op::RecurrentSequenceDirection::FORWARD);

        auto transpose_after_const = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 4 }, { 2, 1, 0, 3 });
        auto transpose_after = std::make_shared<ngraph::opset1::Transpose>(lstm_seq->output(0), transpose_after_const);

        const auto Y_out = std::make_shared<ngraph::opset1::Result>(transpose_after);
        const auto Ho = std::make_shared<ngraph::opset1::Result>(lstm_seq->output(1));
        const auto Co = std::make_shared<ngraph::opset1::Result>(lstm_seq->output(2));
        Y_out->set_friendly_name("Y_out");
        Ho->set_friendly_name("Ho");
        Co->set_friendly_name("Co");

        f = std::make_shared<ngraph::Function>(ngraph::ResultVector{ Y_out, Ho, Co }, ngraph::ParameterVector{ X, Y, Z });

        ngraph::pass::Manager m;
        m.register_pass<ngraph::pass::InitNodeInfo>();
        m.register_pass<OptimizeLSTMSequenceTransposes>();
        m.run_passes(f);
    }

    {
        auto X = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::Shape{ 2, 1, 16 });
        auto Y = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::Shape{ 1, 1, 128 });
        auto Z = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::Shape{ 1, 1, 128 });

        auto w_val = std::vector<float>(512 * 16, 0);
        auto r_val = std::vector<float>(512 * 128, 0);
        auto b_val = std::vector<float>(512, 0);
        auto W = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 512, 16 }, w_val);
        auto R = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 512, 128 }, r_val);
        auto B = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 512 }, b_val);

        auto reshape_before_const  = ngraph::opset1::Constant::create(ngraph::element::i64, ngraph::Shape{ 3 }, { 1, 2, 16 });
        auto reshape_before = std::make_shared<ngraph::opset1::Reshape>(X, reshape_before_const, false);

        auto seq_lengths = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 1 }, { 2 });
        auto lstm_seq = std::make_shared<ngraph::opset5::LSTMSequence>(reshape_before, Y, Z, seq_lengths, W, R, B, 128,
            ngraph::op::RecurrentSequenceDirection::FORWARD);

        auto reshape_after_const = ngraph::opset1::Constant::create(ngraph::element::i64, ngraph::Shape{ 4 }, { 2, 1, 1, 128 });
        auto reshape_after = std::make_shared<ngraph::opset1::Reshape>(lstm_seq->output(0), reshape_after_const, false);

        const auto Y_out = std::make_shared<ngraph::opset1::Result>(reshape_after);
        const auto Ho = std::make_shared<ngraph::opset1::Result>(lstm_seq->output(1));
        const auto Co = std::make_shared<ngraph::opset1::Result>(lstm_seq->output(2));
        Y_out->set_friendly_name("Y_out");
        Ho->set_friendly_name("Ho");
        Co->set_friendly_name("Co");

        f_ref = std::make_shared<ngraph::Function>(ngraph::ResultVector{ Y_out, Ho, Co }, ngraph::ParameterVector{ X, Y, Z });
    }

    auto res = compare_functions(f, f_ref);
    ASSERT_TRUE(res.first) << res.second;
}

TEST(TransformationTests, OptimizeLSTMSequenceTransposesDynamicTest) {
    std::shared_ptr<ngraph::Function> f(nullptr), f_ref(nullptr);
    {
        auto X = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::PartialShape{ 2, -1, -1 });
        auto Y = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::PartialShape{ 1, 1, 128 });
        auto Z = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::PartialShape{ 1, 1, 128 });

        auto w_val = std::vector<float>(512 * 16, 0);
        auto r_val = std::vector<float>(512 * 128, 0);
        auto b_val = std::vector<float>(512, 0);
        auto W = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 512, 16 }, w_val);
        auto R = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 512, 128 }, r_val);
        auto B = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 512 }, b_val);

        auto transpose_before_const = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 3 }, { 1, 0, 2 });
        auto transpose_before = std::make_shared<ngraph::opset1::Transpose>(X, transpose_before_const);

        auto seq_lengths = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 1 }, { 2 });
        auto lstm_seq = std::make_shared<ngraph::opset5::LSTMSequence>(transpose_before, Y, Z, seq_lengths, W, R, B, 128,
            ngraph::op::RecurrentSequenceDirection::FORWARD);

        auto transpose_after_const = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 4 }, { 2, 1, 0, 3 });
        auto transpose_after = std::make_shared<ngraph::opset1::Transpose>(lstm_seq->output(0), transpose_after_const);

        const auto Y_out = std::make_shared<ngraph::opset1::Result>(transpose_after);
        const auto Ho = std::make_shared<ngraph::opset1::Result>(lstm_seq->output(1));
        const auto Co = std::make_shared<ngraph::opset1::Result>(lstm_seq->output(2));
        Y_out->set_friendly_name("Y_out");
        Ho->set_friendly_name("Ho");
        Co->set_friendly_name("Co");

        f = std::make_shared<ngraph::Function>(ngraph::ResultVector{ Y_out, Ho, Co }, ngraph::ParameterVector{ X, Y, Z });

        ngraph::pass::Manager m;
        m.register_pass<ngraph::pass::InitNodeInfo>();
        m.register_pass<OptimizeLSTMSequenceTransposes>();
        m.run_passes(f);
    }

    {
        auto X = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::PartialShape{ 2, -1, -1 });
        auto Y = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::PartialShape{ 1, 1, 128 });
        auto Z = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::PartialShape{ 1, 1, 128 });

        auto w_val = std::vector<float>(512 * 16, 0);
        auto r_val = std::vector<float>(512 * 128, 0);
        auto b_val = std::vector<float>(512, 0);
        auto W = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 512, 16 }, w_val);
        auto R = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 512, 128 }, r_val);
        auto B = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 512 }, b_val);

        auto data = std::make_shared<ngraph::opset1::ShapeOf>(X);
        auto reshape_before_pattern = std::make_shared<ngraph::opset1::Gather>(data,
            ngraph::opset1::Constant::create(ngraph::element::i32, { 3 }, { 1, 0, 2 }),
            ngraph::opset1::Constant::create(ngraph::element::i32, {}, { 0 }));
        auto reshape_before = std::make_shared<ngraph::opset1::Reshape>(X, reshape_before_pattern, false);

        auto seq_lengths = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 1 }, { 2 });
        auto lstm_seq = std::make_shared<ngraph::opset5::LSTMSequence>(reshape_before, Y, Z, seq_lengths, W, R, B, 128,
            ngraph::op::RecurrentSequenceDirection::FORWARD);

        auto reshape_after_const = ngraph::opset1::Constant::create(ngraph::element::i64, ngraph::Shape{ 4 }, { 2, 1, 1, 128 });
        auto reshape_after = std::make_shared<ngraph::opset1::Reshape>(lstm_seq->output(0), reshape_after_const, false);

        const auto Y_out = std::make_shared<ngraph::opset1::Result>(reshape_after);
        const auto Ho = std::make_shared<ngraph::opset1::Result>(lstm_seq->output(1));
        const auto Co = std::make_shared<ngraph::opset1::Result>(lstm_seq->output(2));
        Y_out->set_friendly_name("Y_out");
        Ho->set_friendly_name("Ho");
        Co->set_friendly_name("Co");

        f_ref = std::make_shared<ngraph::Function>(ngraph::ResultVector{ Y_out, Ho, Co }, ngraph::ParameterVector{ X, Y, Z });
    }

    auto res = compare_functions(f, f_ref);
    ASSERT_TRUE(res.first) << res.second;
}

TEST(TransformationTests, OptimizeRNNSequenceTransposesTest) {
    std::shared_ptr<ngraph::Function> f(nullptr), f_ref(nullptr);
    {
        auto X = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::Shape{ 2, 1, 16 });
        auto Y = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::Shape{ 1, 1, 128 });

        auto w_val = std::vector<float>(128 * 16, 0);
        auto r_val = std::vector<float>(128 * 128, 0);
        auto b_val = std::vector<float>(128, 0);
        auto W = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 128, 16 }, w_val);
        auto R = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 128, 128 }, r_val);
        auto B = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 128 }, b_val);

        auto transpose_before_const = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 3 }, { 1, 0, 2 });
        auto transpose_before = std::make_shared<ngraph::opset1::Transpose>(X, transpose_before_const);

        auto seq_lengths = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 1 }, { 2 });
        auto lstm_seq = std::make_shared<ngraph::opset5::RNNSequence>(transpose_before, Y, seq_lengths, W, R, B, 128,
            ngraph::op::RecurrentSequenceDirection::FORWARD);

        auto transpose_after_const = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 4 }, { 2, 1, 0, 3 });
        auto transpose_after = std::make_shared<ngraph::opset1::Transpose>(lstm_seq->output(0), transpose_after_const);

        const auto Y_out = std::make_shared<ngraph::opset1::Result>(transpose_after);
        const auto Ho = std::make_shared<ngraph::opset1::Result>(lstm_seq->output(1));
        Y_out->set_friendly_name("Y_out");
        Ho->set_friendly_name("Ho");

        f = std::make_shared<ngraph::Function>(ngraph::ResultVector{ Y_out, Ho }, ngraph::ParameterVector{ X, Y });

        ngraph::pass::Manager m;
        m.register_pass<ngraph::pass::InitNodeInfo>();
        m.register_pass<OptimizeRNNSequenceTransposes>();
        m.run_passes(f);
    }

    {
        auto X = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::Shape{ 2, 1, 16 });
        auto Y = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::Shape{ 1, 1, 128 });

        auto w_val = std::vector<float>(128 * 16, 0);
        auto r_val = std::vector<float>(128 * 128, 0);
        auto b_val = std::vector<float>(128, 0);
        auto W = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 128, 16 }, w_val);
        auto R = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 128, 128 }, r_val);
        auto B = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 128 }, b_val);

        auto reshape_before_const = ngraph::opset1::Constant::create(ngraph::element::i64, ngraph::Shape{ 3 }, { 1, 2, 16 });
        auto reshape_before = std::make_shared<ngraph::opset1::Reshape>(X, reshape_before_const, false);

        auto seq_lengths = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 1 }, { 2 });
        auto lstm_seq = std::make_shared<ngraph::opset5::RNNSequence>(reshape_before, Y, seq_lengths, W, R, B, 128,
            ngraph::op::RecurrentSequenceDirection::FORWARD);

        auto reshape_after_const = ngraph::opset1::Constant::create(ngraph::element::i64, ngraph::Shape{ 4 }, { 2, 1, 1, 128 });
        auto reshape_after = std::make_shared<ngraph::opset1::Reshape>(lstm_seq->output(0), reshape_after_const, false);

        const auto Y_out = std::make_shared<ngraph::opset1::Result>(reshape_after);
        const auto Ho = std::make_shared<ngraph::opset1::Result>(lstm_seq->output(1));
        Y_out->set_friendly_name("Y_out");
        Ho->set_friendly_name("Ho");

        f_ref = std::make_shared<ngraph::Function>(ngraph::ResultVector{ Y_out, Ho }, ngraph::ParameterVector{ X, Y });
    }

    auto res = compare_functions(f, f_ref);
    ASSERT_TRUE(res.first) << res.second;
}

TEST(TransformationTests, OptimizeRNNSequenceTransposesDynamicTest) {
    std::shared_ptr<ngraph::Function> f(nullptr), f_ref(nullptr);
    {
        auto X = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::PartialShape{ 2, -1, -1 });
        auto Y = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::Shape{ 1, 1, 128 });

        auto w_val = std::vector<float>(128 * 16, 0);
        auto r_val = std::vector<float>(128 * 128, 0);
        auto b_val = std::vector<float>(128, 0);
        auto W = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 128, 16 }, w_val);
        auto R = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 128, 128 }, r_val);
        auto B = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 128 }, b_val);

        auto transpose_before_const = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 3 }, { 1, 0, 2 });
        auto transpose_before = std::make_shared<ngraph::opset1::Transpose>(X, transpose_before_const);

        auto seq_lengths = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 1 }, { 2 });
        auto lstm_seq = std::make_shared<ngraph::opset5::RNNSequence>(transpose_before, Y, seq_lengths, W, R, B, 128,
            ngraph::op::RecurrentSequenceDirection::FORWARD);

        auto transpose_after_const = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 4 }, { 2, 1, 0, 3 });
        auto transpose_after = std::make_shared<ngraph::opset1::Transpose>(lstm_seq->output(0), transpose_after_const);

        const auto Y_out = std::make_shared<ngraph::opset1::Result>(transpose_after);
        const auto Ho = std::make_shared<ngraph::opset1::Result>(lstm_seq->output(1));
        Y_out->set_friendly_name("Y_out");
        Ho->set_friendly_name("Ho");

        f = std::make_shared<ngraph::Function>(ngraph::ResultVector{ Y_out, Ho }, ngraph::ParameterVector{ X, Y });

        ngraph::pass::Manager m;
        m.register_pass<ngraph::pass::InitNodeInfo>();
        m.register_pass<OptimizeRNNSequenceTransposes>();
        m.run_passes(f);
    }

    {
        auto X = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::PartialShape{ 2, -1, -1 });
        auto Y = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::Shape{ 1, 1, 128 });

        auto w_val = std::vector<float>(128 * 16, 0);
        auto r_val = std::vector<float>(128 * 128, 0);
        auto b_val = std::vector<float>(128, 0);
        auto W = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 128, 16 }, w_val);
        auto R = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 128, 128 }, r_val);
        auto B = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 128 }, b_val);

        auto data = std::make_shared<ngraph::opset1::ShapeOf>(X);
        auto reshape_before_pattern = std::make_shared<ngraph::opset1::Gather>(data,
            ngraph::opset1::Constant::create(ngraph::element::i32, { 3 }, { 1, 0, 2 }),
            ngraph::opset1::Constant::create(ngraph::element::i32, {}, { 0 }));
        auto reshape_before = std::make_shared<ngraph::opset1::Reshape>(X, reshape_before_pattern, false);

        auto seq_lengths = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 1 }, { 2 });
        auto lstm_seq = std::make_shared<ngraph::opset5::RNNSequence>(reshape_before, Y, seq_lengths, W, R, B, 128,
            ngraph::op::RecurrentSequenceDirection::FORWARD);

        auto reshape_after_const = ngraph::opset1::Constant::create(ngraph::element::i64, ngraph::Shape{ 4 }, { 2, 1, 1, 128 });
        auto reshape_after = std::make_shared<ngraph::opset1::Reshape>(lstm_seq->output(0), reshape_after_const, false);

        const auto Y_out = std::make_shared<ngraph::opset1::Result>(reshape_after);
        const auto Ho = std::make_shared<ngraph::opset1::Result>(lstm_seq->output(1));
        Y_out->set_friendly_name("Y_out");
        Ho->set_friendly_name("Ho");

        f_ref = std::make_shared<ngraph::Function>(ngraph::ResultVector{ Y_out, Ho }, ngraph::ParameterVector{ X, Y });
    }

    auto res = compare_functions(f, f_ref);
    ASSERT_TRUE(res.first) << res.second;
}

TEST(TransformationTests, OptimizeGRUSequenceTransposesTest) {
    std::shared_ptr<ngraph::Function> f(nullptr), f_ref(nullptr);
    {
        auto X = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::Shape{ 2, 1, 16 });
        auto Y = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::Shape{ 1, 1, 128 });

        auto w_val = std::vector<float>(384 * 16, 0);
        auto r_val = std::vector<float>(384 * 128, 0);
        auto b_val = std::vector<float>(384, 0);
        auto W = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 384, 16 }, w_val);
        auto R = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 384, 128 }, r_val);
        auto B = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 384 }, b_val);

        auto transpose_before_const = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 3 }, { 1, 0, 2 });
        auto transpose_before = std::make_shared<ngraph::opset1::Transpose>(X, transpose_before_const);

        auto seq_lengths = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 1 }, { 2 });
        auto lstm_seq = std::make_shared<ngraph::opset5::GRUSequence>(transpose_before, Y, seq_lengths, W, R, B, 128,
            ngraph::op::RecurrentSequenceDirection::FORWARD);

        auto transpose_after_const = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 4 }, { 2, 1, 0, 3 });
        auto transpose_after = std::make_shared<ngraph::opset1::Transpose>(lstm_seq->output(0), transpose_after_const);

        const auto Y_out = std::make_shared<ngraph::opset1::Result>(transpose_after);
        const auto Ho = std::make_shared<ngraph::opset1::Result>(lstm_seq->output(1));
        Y_out->set_friendly_name("Y_out");
        Ho->set_friendly_name("Ho");

        f = std::make_shared<ngraph::Function>(ngraph::ResultVector{ Y_out, Ho }, ngraph::ParameterVector{ X, Y });

        ngraph::pass::Manager m;
        m.register_pass<ngraph::pass::InitNodeInfo>();
        m.register_pass<OptimizeGRUSequenceTransposes>();
        m.run_passes(f);
    }

    {
        auto X = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::Shape{ 2, 1, 16 });
        auto Y = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::Shape{ 1, 1, 128 });

        auto w_val = std::vector<float>(384 * 16, 0);
        auto r_val = std::vector<float>(384 * 128, 0);
        auto b_val = std::vector<float>(384, 0);
        auto W = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 384, 16 }, w_val);
        auto R = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 384, 128 }, r_val);
        auto B = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 384 }, b_val);

        auto reshape_before_const = ngraph::opset1::Constant::create(ngraph::element::i64, ngraph::Shape{ 3 }, { 1, 2, 16 });
        auto reshape_before = std::make_shared<ngraph::opset1::Reshape>(X, reshape_before_const, false);

        auto seq_lengths = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 1 }, { 2 });
        auto lstm_seq = std::make_shared<ngraph::opset5::GRUSequence>(reshape_before, Y, seq_lengths, W, R, B, 128,
            ngraph::op::RecurrentSequenceDirection::FORWARD);

        auto reshape_after_const = ngraph::opset1::Constant::create(ngraph::element::i64, ngraph::Shape{ 4 }, { 2, 1, 1, 128 });
        auto reshape_after = std::make_shared<ngraph::opset1::Reshape>(lstm_seq->output(0), reshape_after_const, false);

        const auto Y_out = std::make_shared<ngraph::opset1::Result>(reshape_after);
        const auto Ho = std::make_shared<ngraph::opset1::Result>(lstm_seq->output(1));
        Y_out->set_friendly_name("Y_out");
        Ho->set_friendly_name("Ho");

        f_ref = std::make_shared<ngraph::Function>(ngraph::ResultVector{ Y_out, Ho }, ngraph::ParameterVector{ X, Y });
    }

    auto res = compare_functions(f, f_ref);
    ASSERT_TRUE(res.first) << res.second;
}

TEST(TransformationTests, OptimizeGRUSequenceTransposesDynamicTest) {
    std::shared_ptr<ngraph::Function> f(nullptr), f_ref(nullptr);
    {
        auto X = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::PartialShape{ 2, -1, -1 });
        auto Y = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::Shape{ 1, 1, 128 });

        auto w_val = std::vector<float>(384 * 16, 0);
        auto r_val = std::vector<float>(384 * 128, 0);
        auto b_val = std::vector<float>(384, 0);
        auto W = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 384, 16 }, w_val);
        auto R = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 384, 128 }, r_val);
        auto B = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 384 }, b_val);

        auto transpose_before_const = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 3 }, { 1, 0, 2 });
        auto transpose_before = std::make_shared<ngraph::opset1::Transpose>(X, transpose_before_const);

        auto seq_lengths = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 1 }, { 2 });
        auto lstm_seq = std::make_shared<ngraph::opset5::GRUSequence>(transpose_before, Y, seq_lengths, W, R, B, 128,
            ngraph::op::RecurrentSequenceDirection::FORWARD);

        auto transpose_after_const = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 4 }, { 2, 1, 0, 3 });
        auto transpose_after = std::make_shared<ngraph::opset1::Transpose>(lstm_seq->output(0), transpose_after_const);

        const auto Y_out = std::make_shared<ngraph::opset1::Result>(transpose_after);
        const auto Ho = std::make_shared<ngraph::opset1::Result>(lstm_seq->output(1));
        Y_out->set_friendly_name("Y_out");
        Ho->set_friendly_name("Ho");

        f = std::make_shared<ngraph::Function>(ngraph::ResultVector{ Y_out, Ho }, ngraph::ParameterVector{ X, Y });

        ngraph::pass::Manager m;
        m.register_pass<ngraph::pass::InitNodeInfo>();
        m.register_pass<OptimizeGRUSequenceTransposes>();
        m.run_passes(f);
    }

    {
        auto X = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::PartialShape{ 2, -1, -1 });
        auto Y = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::Shape{ 1, 1, 128 });

        auto w_val = std::vector<float>(384 * 16, 0);
        auto r_val = std::vector<float>(384 * 128, 0);
        auto b_val = std::vector<float>(384, 0);
        auto W = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 384, 16 }, w_val);
        auto R = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 384, 128 }, r_val);
        auto B = ngraph::opset1::Constant::create(ngraph::element::f32, ngraph::Shape{ 1, 384 }, b_val);

        auto data = std::make_shared<ngraph::opset1::ShapeOf>(X);
        auto reshape_before_pattern = std::make_shared<ngraph::opset1::Gather>(data,
            ngraph::opset1::Constant::create(ngraph::element::i32, { 3 }, { 1, 0, 2 }),
            ngraph::opset1::Constant::create(ngraph::element::i32, {}, { 0 }));
        auto reshape_before = std::make_shared<ngraph::opset1::Reshape>(X, reshape_before_pattern, false);

        auto seq_lengths = ngraph::opset1::Constant::create(ngraph::element::i32, ngraph::Shape{ 1 }, { 2 });
        auto lstm_seq = std::make_shared<ngraph::opset5::GRUSequence>(reshape_before, Y, seq_lengths, W, R, B, 128,
            ngraph::op::RecurrentSequenceDirection::FORWARD);

        auto reshape_after_const = ngraph::opset1::Constant::create(ngraph::element::i64, ngraph::Shape{ 4 }, { 2, 1, 1, 128 });
        auto reshape_after = std::make_shared<ngraph::opset1::Reshape>(lstm_seq->output(0), reshape_after_const, false);

        const auto Y_out = std::make_shared<ngraph::opset1::Result>(reshape_after);
        const auto Ho = std::make_shared<ngraph::opset1::Result>(lstm_seq->output(1));
        Y_out->set_friendly_name("Y_out");
        Ho->set_friendly_name("Ho");

        f_ref = std::make_shared<ngraph::Function>(ngraph::ResultVector{ Y_out, Ho }, ngraph::ParameterVector{ X, Y });
    }

    auto res = compare_functions(f, f_ref);
    ASSERT_TRUE(res.first) << res.second;
}
