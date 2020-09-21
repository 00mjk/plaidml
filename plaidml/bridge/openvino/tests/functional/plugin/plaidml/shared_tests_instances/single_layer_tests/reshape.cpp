// Copyright (C) 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "single_layer_tests/reshape.hpp"
#include "common_test_utils/test_constants.hpp"
#include <vector>

using LayerTestsDefinitions::ReshapeLayerTest;

namespace {
const std::vector<InferenceEngine::Precision> netPrecisions = {
    InferenceEngine::Precision::FP32,
    // InferenceEngine::Precision::FP16,
    // InferenceEngine::Precision::I64
};

INSTANTIATE_TEST_SUITE_P(ReshapeCheckDynBatch, ReshapeLayerTest,
                         ::testing::Combine(::testing::Values(true),                                      //
                                            ::testing::ValuesIn(netPrecisions),                           //
                                            ::testing::Values(std::vector<size_t>({1, 16, 16, 16})),      //
                                            ::testing::Values(std::vector<size_t>({1, 0, 256})),          //
                                            ::testing::Values(CommonTestUtils::DEVICE_PLAIDML),           //
                                            ::testing::Values(std::map<std::string, std::string>(         //
                                                {{CONFIG_KEY(DYN_BATCH_ENABLED), CONFIG_VALUE(YES)}}))),  //
                         ReshapeLayerTest::getTestCaseName);

INSTANTIATE_TEST_SUITE_P(ReshapeCheck, ReshapeLayerTest,
                         ::testing::Combine(::testing::Values(true),                                     //
                                            ::testing::ValuesIn(netPrecisions),                          //
                                            ::testing::Values(std::vector<size_t>({10, 10, 10, 10})),    //
                                            ::testing::Values(std::vector<size_t>({10, 0, 100})),        //
                                            ::testing::Values(CommonTestUtils::DEVICE_PLAIDML),          //
                                            ::testing::Values(std::map<std::string, std::string>({}))),  //
                         ReshapeLayerTest::getTestCaseName);
}  // namespace