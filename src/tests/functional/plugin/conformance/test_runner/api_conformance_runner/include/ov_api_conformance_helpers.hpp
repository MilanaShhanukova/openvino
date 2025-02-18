// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "conformance.hpp"
#include "common_test_utils/test_constants.hpp"

// TODO: fix namespaces

namespace ov {
namespace test {
namespace conformance {

inline const std::string get_plugin_lib_name_by_device(const std::string& deviceName) {
    const std::map<std::string, std::string> devices{
            { "AUTO", "ov_auto_plugin" },
            { "HDDL", "HDDLPlugin" },
            { "VPUX", "ov_intel_vpux_plugin" },
            { "AUTO", "ov_auto_plugin" },
            { "CPU", "ov_intel_cpu_plugin" },
            { "GNA", "ov_intel_gna_plugin" },
            { "GPU", "ov_intel_gpu_plugin" },
            { "HETERO", "ov_hetero_plugin" },
            { "MULTI", "ov_multi_plugin" },
            { "MYRIAD", "ov_intel_vpu_plugin" },
            { "TEMPLATE", "ov_template_plugin" },
    };
    if (devices.find(deviceName) == devices.end()) {
        throw std::runtime_error("Incorrect device name");
    }
    return devices.at(deviceName);
}


inline const std::vector<ov::AnyMap> generate_configs(const std::string& targetDevice,
                                                                         const std::vector<ov::AnyMap>& config = {}) {
    std::pair<std::string, std::string> defaultConfig;
    if (targetDevice ==  std::string(CommonTestUtils::DEVICE_MULTI) || targetDevice ==  std::string(CommonTestUtils::DEVICE_AUTO)) {
        defaultConfig = {MULTI_CONFIG_KEY(DEVICE_PRIORITIES), ov::test::conformance::targetDevice};;
    } else if (targetDevice ==  std::string(CommonTestUtils::DEVICE_HETERO)) {
        defaultConfig = { "TARGET_FALLBACK" , ov::test::conformance::targetDevice };
    } else if (targetDevice ==  std::string(CommonTestUtils::DEVICE_BATCH)) {
        defaultConfig = { CONFIG_KEY(AUTO_BATCH_DEVICE_CONFIG) , std::string(ov::test::conformance::targetDevice)};
    } else {
        throw std::runtime_error("Incorrect target device: " + targetDevice);
    }

    std::vector<ov::AnyMap> resultConfig;
    if (config.empty()) {
        return {{defaultConfig}};
    }
    for (auto configItem : config) {
        configItem.insert(defaultConfig);
        resultConfig.push_back(configItem);
    }
    return resultConfig;
}

inline const std::string generate_complex_device_name(const std::string& deviceName) {
    return deviceName + ":" + ov::test::conformance::targetDevice;
}

inline const std::vector<std::string> return_all_possible_device_combination() {
    std::vector<std::string> res{ov::test::conformance::targetDevice};
    std::vector<std::string> devices{CommonTestUtils::DEVICE_HETERO, CommonTestUtils::DEVICE_AUTO, CommonTestUtils::DEVICE_MULTI};
    for (const auto& device : devices) {
        res.emplace_back(generate_complex_device_name(device));
    }
    return res;
}

const std::vector<ov::AnyMap> empty_config = {
        {},
};

}  // namespace conformance
}  // namespace test
}  // namespace ov
