/* -*- c++ -*- */
/*
 * Copyright 2018 Florian Lotze
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "bitcontainertest.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <numeric>

CPPUNIT_TEST_SUITE_REGISTRATION(BitContainerTest);

void BitContainerTest::setUp()
{
    mTestData = "TestData";
    mFrozenBits = { 0, 1, 2, 3, 4, 5, 6, 7 }; // Freeze first byte

    floatContainer = new PolarCode::FloatContainer(mTestData.size() * 8, mFrozenBits);
    charContainer = new PolarCode::CharContainer(mTestData.size() * 8, mFrozenBits);
    packedContainer = new PolarCode::PackedContainer(mTestData.size() * 8, mFrozenBits);

    control = new unsigned char[mTestData.size()];
}

void BitContainerTest::tearDown()
{
    delete floatContainer;
    delete charContainer;
    delete packedContainer;
    delete[] control;
}

void BitContainerTest::testFloatContainer()
{
    memset(control, 0, mTestData.size());
    floatContainer->insertPackedBits(mTestData.data());
    floatContainer->getPackedBits(control);

    CPPUNIT_ASSERT(memcmp(mTestData.data(), control, mTestData.size()) == 0);
}

void BitContainerTest::testFloatContainerWithFrozenBits()
{
    memset(control, 0, mTestData.size());
    floatContainer->insertPackedInformationBits(mTestData.data());
    floatContainer->getPackedInformationBits(control);

    CPPUNIT_ASSERT(memcmp(mTestData.data(), control, mTestData.size() - 1) == 0);
}

void BitContainerTest::testCharContainer()
{
    memset(control, 0, mTestData.size());
    charContainer->insertPackedBits(mTestData.data());
    charContainer->getPackedBits(control);

    CPPUNIT_ASSERT(memcmp(mTestData.data(), control, mTestData.size()) == 0);
}

void BitContainerTest::testCharContainerWithFrozenBits()
{
    memset(control, 0, mTestData.size());
    charContainer->insertPackedInformationBits(mTestData.data());
    charContainer->getPackedInformationBits(control);

    CPPUNIT_ASSERT(memcmp(mTestData.data(), control, mTestData.size() - 1) == 0);
}

void BitContainerTest::testCharContainerWithFloatInputLarge()
{
    const unsigned size = 64;
    const float scale = 4.0;
    std::vector<float> input(size);
    std::vector<char> expected(size);
    std::iota(input.begin(), input.end(), -4);
    for (unsigned i = 0; i < size; ++i) {
        input[i] = input[i] * scale + 0.3f;
        const float limited =
            std::max(float(INT8_MIN), std::min(float(INT8_MAX), std::round(input[i])));
        expected[i] = (char)limited;
    }

    std::vector<char> output(size);

    auto container = std::make_unique<PolarCode::CharContainer>(size);
    container->insertLlr(input.data());
    container->getSoftBits((void*)output.data());

    for (unsigned i = 0; i < size; ++i) {
        CPPUNIT_ASSERT_EQUAL(expected[i], output[i]);
    }
}

void BitContainerTest::testPackedContainer()
{
    memset(control, 0, mTestData.size());
    packedContainer->insertPackedBits(mTestData.data());
    packedContainer->getPackedBits(control);

    CPPUNIT_ASSERT(memcmp(mTestData.data(), control, mTestData.size()) == 0);
}

void BitContainerTest::testPackedContainerWithFrozenBits()
{
    memset(control, 0, mTestData.size());
    packedContainer->insertPackedInformationBits(mTestData.data());
    packedContainer->getPackedInformationBits(control);

    CPPUNIT_ASSERT(memcmp(mTestData.data(), control, mTestData.size() - 1) == 0);
}
