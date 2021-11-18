// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#ifdef OPENCV_TEMPLATE_TESTS

#include <gtest/gtest.h>

#include <opencv2/imgproc.hpp>
#include <opencv2/imgproc/types_c.h>

#include <openvino/core/preprocess/pre_post_process.hpp>
#include <shared_test_classes/base/layer_test_utils.hpp>
#include <shared_test_classes/single_layer/convert_color_nv12.hpp>

#include "base_reference_test.hpp"
#include "ngraph_functions/builders.hpp"

using namespace ov;
using namespace ov::preprocess;
using namespace reference_tests;
namespace {

class PreprocessOpenCVReferenceTest : public testing::Test, public CommonReferenceTest {
public:
    void SetUp() override {
        SKIP_IF_CURRENT_TEST_IS_DISABLED()
    }
};

class PreprocessOpenCVReferenceTest_NV12 : public PreprocessOpenCVReferenceTest {
public:
    void Validate() override {
        threshold = 1.f;
        abs_threshold = 1.f;
        // No pixels with deviation of more than 1 color step
        CommonReferenceTest::Validate();
        // Less than 2% of deviations with 1 color step. 2% is experimental value
        // For very precise (acceptable) float calculations - 1.4% deviation with G-API/OpenCV is observed
        LayerTestsDefinitions::NV12TestUtils::ValidateColors(refOutData[0].data<uint8_t>(),
                actualOutData[0].data<uint8_t>(), refOutData[0].get_size(), 0.02);
    }
};

} // namespace

static std::shared_ptr<Function> create_simple_function(element::Type type, const PartialShape& shape) {
    auto data1 = std::make_shared<op::v0::Parameter>(type, shape);
    data1->set_friendly_name("input1");
    data1->get_output_tensor(0).set_names({"tensor_input1", "input1"});
    auto c = op::v0::Constant::create(type, {1}, {0});
    auto op = std::make_shared<op::v1::Add>(data1, c);
    op->set_friendly_name("Add0");
    auto res = std::make_shared<op::v0::Result>(op);
    res->set_friendly_name("Result1");
    res->get_output_tensor(0).set_names({"tensor_output1", "Result1"});
    return std::make_shared<ov::Function>(ResultVector{res}, ParameterVector{data1});
}

TEST_F(PreprocessOpenCVReferenceTest_NV12, convert_nv12_full_color_range) {
    size_t height = 64; // 64/2 = 32 values for R
    size_t width = 64;  // 64/2 = 32 values for G
    int b_step = 5;
    int b_dim = 255 / b_step + 1;

    // Test various possible r/g/b values within dimensions
    auto ov20_input_yuv = LayerTestsDefinitions::NV12TestUtils::color_test_image(height, width, b_step);

    auto full_height = height * b_dim;
    auto func_shape = Shape{1, full_height, width, 3};
    function = create_simple_function(element::u8, func_shape);

    inputData.clear();

    function = PrePostProcessor(function).input(InputInfo()
                                                .tensor(InputTensorInfo().set_color_format(
                                                        ColorFormat::NV12_SINGLE_PLANE))
                                                .preprocess(PreProcessSteps().convert_color(ColorFormat::BGR)))
            .build();

    const auto &param = function->get_parameters()[0];
    inputData.emplace_back(param->get_element_type(), param->get_shape(), ov20_input_yuv.data());

    // Calculate reference expected values from OpenCV
    cv::Mat picYV12 = cv::Mat(static_cast<int>(full_height) * 3 / 2,
                              static_cast<int>(width),
                              CV_8UC1,
                              ov20_input_yuv.data());
    cv::Mat picBGR;
    cv::cvtColor(picYV12, picBGR, CV_YUV2BGR_NV12);
    refOutData.emplace_back(param->get_element_type(), func_shape, picBGR.data);

    // Exec now
    Exec();
}

TEST_F(PreprocessOpenCVReferenceTest_NV12, convert_nv12_colored) {
    auto input_yuv = std::vector<uint8_t> {235, 81, 235, 81, 109, 184};
    auto func_shape = Shape{1, 2, 2, 3};
    function = create_simple_function(element::u8, func_shape);

    inputData.clear();

    function = PrePostProcessor(function).input(InputInfo()
                                                .tensor(InputTensorInfo().set_color_format(
                                                        ColorFormat::NV12_SINGLE_PLANE))
                                                .preprocess(PreProcessSteps().convert_color(ColorFormat::BGR))
                                                )
            .build();

    const auto &param = function->get_parameters()[0];
    inputData.emplace_back(param->get_element_type(), param->get_shape(), input_yuv.data());

    // Calculate reference expected values from OpenCV
    cv::Mat picYV12 = cv::Mat(3, 2, CV_8UC1, input_yuv.data());
    cv::Mat picBGR;
    cv::cvtColor(picYV12, picBGR, CV_YUV2BGR_NV12);
    refOutData.emplace_back(param->get_element_type(), func_shape, picBGR.data);
    // Exec now
    Exec();
}

TEST_F(PreprocessOpenCVReferenceTest, resize_u8_simple_linear) {
    auto input_shape = Shape{1, 1, 2, 2};
    auto func_shape = Shape{1, 1, 1, 1};
    auto input_img = std::vector<uint8_t> {5, 5, 5, 4};
    function = create_simple_function(element::u8, func_shape);

    inputData.clear();

    function = PrePostProcessor(function).input(InputInfo()
                                                .tensor(InputTensorInfo().set_spatial_static_shape(2, 2))
                                                .preprocess(PreProcessSteps().resize(ResizeAlgorithm::RESIZE_LINEAR))
                                                .network(InputNetworkInfo().set_layout("NCHW"))
            )
            .build();

    const auto &param = function->get_parameters()[0];
    inputData.emplace_back(param->get_element_type(), param->get_shape(), input_img.data());

    // Calculate reference expected values from OpenCV
    cv::Mat cvPic = cv::Mat(2, 2, CV_8UC1, input_img.data());
    cv::Mat cvPicResized;
    cv::resize(cvPic, cvPicResized, cv::Size(1, 1), cv::INTER_NEAREST);
    refOutData.emplace_back(param->get_element_type(), func_shape, cvPicResized.data);
    // Exec now
    Exec();
}

TEST_F(PreprocessOpenCVReferenceTest, resize_u8_large_picture_linear) {
    threshold = 1.f;
    abs_threshold = 1.f; // Some pixels still have deviations of 1 step
    const size_t input_height = 50;
    const size_t input_width = 50;
    const size_t func_height = 37;
    const size_t func_width = 31;
    auto input_shape = Shape{1, 1, input_height, input_width};
    auto func_shape = Shape{1, 1, func_height, func_width};
    auto input_img = std::vector<uint8_t> (shape_size(input_shape));
    std::default_random_engine random(0); // hard-coded seed to make test results predictable
    std::uniform_int_distribution<int> distrib(0, 255);
    for (std::size_t i = 0; i < shape_size(input_shape); i++) {
        auto v = distrib(random);
        input_img[i] = static_cast<uint8_t>(v);
    }
    function = create_simple_function(element::u8, func_shape);

    inputData.clear();

    function = PrePostProcessor(function).input(InputInfo()
                                                .tensor(InputTensorInfo().set_spatial_static_shape(input_height, input_width))
                                                .preprocess(PreProcessSteps().resize(ResizeAlgorithm::RESIZE_LINEAR))
                                                .network(InputNetworkInfo().set_layout("NCHW"))
            )
            .build();

    const auto &param = function->get_parameters()[0];
    inputData.emplace_back(param->get_element_type(), param->get_shape(), input_img.data());

    // Calculate reference expected values from OpenCV
    cv::Mat cvPic = cv::Mat(input_height, input_width, CV_8UC1, input_img.data());
    cv::Mat cvPicResized;
    cv::resize(cvPic, cvPicResized, cv::Size(func_width, func_height), cv::INTER_LINEAR_EXACT);
    refOutData.emplace_back(param->get_element_type(), func_shape, cvPicResized.data);
    // Exec now
    Exec();
}

TEST_F(PreprocessOpenCVReferenceTest, resize_f32_large_picture_linear) {
    threshold = 0.01f;
    abs_threshold = 0.01f;
    const size_t input_height = 50;
    const size_t input_width = 50;
    const size_t func_height = 37;
    const size_t func_width = 31;
    auto input_shape = Shape{1, 1, input_height, input_width};
    auto func_shape = Shape{1, 1, func_height, func_width};
    auto input_img = std::vector<float> (shape_size(input_shape));
    std::default_random_engine random(0); // hard-coded seed to make test results predictable
    std::uniform_int_distribution<int> distrib(0, 255);
    for (std::size_t i = 0; i < shape_size(input_shape); i++) {
        input_img[i] = static_cast<float>(distrib(random));
    }
    function = create_simple_function(element::f32, func_shape);

    inputData.clear();

    function = PrePostProcessor(function).input(InputInfo()
                                                .tensor(InputTensorInfo().set_spatial_static_shape(input_height, input_width))
                                                .preprocess(PreProcessSteps().resize(ResizeAlgorithm::RESIZE_LINEAR))
                                                .network(InputNetworkInfo().set_layout("NCHW"))
            )
            .build();

    const auto &param = function->get_parameters()[0];
    inputData.emplace_back(param->get_element_type(), param->get_shape(), input_img.data());

    // Calculate reference expected values from OpenCV
    cv::Mat cvPic = cv::Mat(input_height, input_width, CV_32FC1, input_img.data());
    cv::Mat cvPicResized;
    cv::resize(cvPic, cvPicResized, cv::Size(func_width, func_height), cv::INTER_LINEAR_EXACT);
    refOutData.emplace_back(param->get_element_type(), func_shape, cvPicResized.data);
    // Exec now
    Exec();
}

TEST_F(PreprocessOpenCVReferenceTest, DISABLED_resize_f32_large_picture_cubic_small) {
    const size_t input_height = 4;
    const size_t input_width = 4;
    const size_t func_height = 3;
    const size_t func_width = 3;
    auto input_shape = Shape{1, 1, input_height, input_width};
    auto func_shape = Shape{1, 1, func_height, func_width};
    auto element_type = element::f32;
    auto input_img = std::vector<float> {1.f, 2.f, 3.f, 4.f, 4.f, 3.f, 2.f, 1.f, 1.f, 2.f, 3.f, 4.f, 4.f, 3.f, 2.f, 1.f};
    function = create_simple_function(element_type, func_shape);
    function = PrePostProcessor(function).input(InputInfo()
                                                .tensor(InputTensorInfo().set_spatial_static_shape(input_height, input_width))
                                                .preprocess(PreProcessSteps().resize(ResizeAlgorithm::RESIZE_CUBIC))
                                                .network(InputNetworkInfo().set_layout("NCHW"))
            )
            .build();

    inputData.emplace_back(element_type, input_shape, input_img.data());

    // Calculate reference expected values from OpenCV
    cv::Mat cvPic = cv::Mat(input_height, input_width, CV_32FC1, input_img.data());
    cv::Mat cvPicResized;
    cv::resize(cvPic, cvPicResized, cv::Size(func_width, func_height), cv::INTER_CUBIC);
    refOutData.emplace_back(element_type, func_shape, cvPicResized.data);
    // Exec now
    Exec();
}

#endif // OPENCV_TEMPLATE_TESTS