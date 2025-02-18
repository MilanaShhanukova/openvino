// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <gtest/gtest.h>
#include <threading/ie_executor_manager.hpp>

using namespace ::testing;
using namespace std;
using namespace InferenceEngine;

TEST(ExecutorManagerTests, canCreateSingleExecutorManager) {
    ExecutorManager *executorManager1 = ExecutorManager::getInstance();

    ExecutorManager *executorManager2 = ExecutorManager::getInstance();

    ASSERT_EQ(executorManager1, executorManager2);
}

TEST(ExecutorManagerTests, createDifferentExecutorsForDifferentDevices) {
    ExecutorManagerImpl _manager;
    auto executor1 = _manager.getExecutor("CPU");
    auto executor2 = _manager.getExecutor("GPU");

    ASSERT_NE(executor1, executor2);
    ASSERT_EQ(2, _manager.getExecutorsNumber());
}

TEST(ExecutorManagerTests, returnTheSameExecutorForTheSameDevice) {
    ExecutorManagerImpl _manager;
    auto executor1 = _manager.getExecutor("CPU");
    auto executor2 = _manager.getExecutor("GPU");

    auto executor = _manager.getExecutor("GPU");

    ASSERT_EQ(executor, executor2);
    ASSERT_EQ(2, _manager.getExecutorsNumber());
}
