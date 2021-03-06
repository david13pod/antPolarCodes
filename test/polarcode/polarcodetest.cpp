/* -*- c++ -*- */
/*
 * Copyright 2018 Florian Lotze
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "polarcodetest.h"

#include <polarcode/construction/bhattacharrya.h>
#include <polarcode/decoding/fastssc_fip_char.h>
#include <polarcode/decoding/scl_fip_char.h>
#include <polarcode/encoding/butterfly_fip_packed.h>
#include <polarcode/errordetection/crc32.h>

#include <iomanip>
#include <iostream>
#include <random>

CPPUNIT_TEST_SUITE_REGISTRATION(PolarCodeTest);

void PolarCodeTest::setUp() {}

void PolarCodeTest::tearDown() {}

void bpskModulate(unsigned char* input, char* output, size_t blockLength)
{
    unsigned char currentByte = 0; // Initialize to suppress false warning
    for (unsigned i = 0; i < blockLength; ++i) {
        if (i % 8 == 0) {
            currentByte = *(input++);
        }
        if (currentByte & 0x80) {
            *output = -1;
        } else {
            *output = 1;
        }
        currentByte <<= 1;
        output++;
    }
}


void PolarCodeTest::testAvx2()
{
    const size_t blockLength = 1 << 12;
    const size_t infoLength = blockLength * 3 / 4;

    unsigned char* input = new unsigned char[infoLength / 8];
    unsigned char* inputBlock = new unsigned char[blockLength / 8];
    char* inputSignal = new char[blockLength];
    unsigned char* output = new unsigned char[infoLength / 8];

    PolarCode::Construction::Constructor* constructor =
        new PolarCode::Construction::Bhattacharrya(blockLength, infoLength);
    std::vector<unsigned> frozenBits = constructor->construct();

    PolarCode::Encoding::ButterflyFipPacked* encoder =
        new PolarCode::Encoding::ButterflyFipPacked(blockLength, frozenBits);

    PolarCode::Decoding::FastSscFipChar* decoder =
        new PolarCode::Decoding::FastSscFipChar(blockLength, frozenBits);

    {
        std::mt19937 generator;
        uint32_t* inputPtr = reinterpret_cast<uint32_t*>(input);
        for (unsigned i = 0; i < infoLength / 32; ++i) {
            inputPtr[i] = generator();
        }
    }

    encoder->setInformation(input);
    encoder->encode();
    encoder->getEncodedData(inputBlock);

    { // Test if encoded bits are still recoverable from systematic codeword
        PolarCode::PackedContainer* cont =
            new PolarCode::PackedContainer(blockLength, frozenBits);
        cont->insertPackedBits(inputBlock);
        cont->getPackedInformationBits(output);
        delete cont;
        CPPUNIT_ASSERT(0 == memcmp(input, output, infoLength / 8));
    }

    bpskModulate(inputBlock, inputSignal, blockLength);

    decoder->setSignal(inputSignal);
    CPPUNIT_ASSERT(decoder->decode());
    decoder->getDecodedInformationBits(output);

    CPPUNIT_ASSERT(0 == memcmp(input, output, infoLength / 8));


    delete encoder;
    delete constructor;
    delete[] input;
    delete[] inputSignal;
    delete[] output;
}

void PolarCodeTest::testAvx2List()
{
    const size_t blockLength = 1 << 12;
    const size_t infoLength = blockLength * 3 / 4;
    const size_t pathLimit = 4;

    unsigned char* input = new unsigned char[infoLength / 8];
    unsigned char* inputBlock = new unsigned char[blockLength / 8];
    char* inputSignal = new char[blockLength];
    unsigned char* output = new unsigned char[infoLength / 8];

    PolarCode::Construction::Constructor* constructor =
        new PolarCode::Construction::Bhattacharrya(blockLength, infoLength);
    std::vector<unsigned> frozenBits = constructor->construct();

    PolarCode::Encoding::Encoder* encoder =
        new PolarCode::Encoding::ButterflyFipPacked(blockLength, frozenBits);

    PolarCode::ErrorDetection::Detector* errorDetector =
        new PolarCode::ErrorDetection::CRC32();

    PolarCode::Decoding::Decoder* decoder =
        new PolarCode::Decoding::SclFipChar(blockLength, pathLimit, frozenBits);
    decoder->setErrorDetection(errorDetector);

    {
        // Generate random data
        std::mt19937 generator;
        uint32_t* inputPtr = reinterpret_cast<uint32_t*>(input);
        for (unsigned i = 0; i < infoLength / 32; ++i) {
            inputPtr[i] = generator();
        }

        // Apply outer code for error detection
        errorDetector->generate(input, infoLength / 8);
    }


    encoder->setInformation(input);
    encoder->encode();
    encoder->getEncodedData(inputBlock);

    { // Test if encoded bits are still recoverable from systematic codeword
        PolarCode::BitContainer* cont =
            new PolarCode::PackedContainer(blockLength, frozenBits);
        cont->insertPackedBits(inputBlock);
        cont->getPackedInformationBits(output);
        delete cont;
        CPPUNIT_ASSERT_ASSERTION_PASS_MESSAGE(
            "Encoder or BitContainer failed",
            CPPUNIT_ASSERT(0 == memcmp(input, output, infoLength / 8)));
    }

    bpskModulate(inputBlock, inputSignal, blockLength);

    decoder->setSignal(inputSignal);
    /*CPPUNIT_ASSERT(*/ decoder->decode() /*)*/;
    decoder->getDecodedInformationBits(output);
    bool decoderSuccess = (0 == memcmp(input, output, infoLength / 8));

    /*	{
                    char *outputLlr = new char[infoLength];
                    decoder->getSoftInformation(outputLlr);
                    std::cout << std::endl << "[";
                    for(unsigned i=0; i<infoLength; ++i) {
                            std::cout << std::setw(3) << static_cast<signed>(outputLlr[i])
       << std::setw(0) << " ";
                    }
                    std::cout << "\b]" << std::endl;
                    delete [] outputLlr;
            }*/


    delete encoder;
    delete constructor;
    delete[] input;
    delete[] inputSignal;
    delete[] output;

    CPPUNIT_ASSERT_ASSERTION_PASS_MESSAGE("Decoder failed",
                                          CPPUNIT_ASSERT(decoderSuccess));
}

void PolarCodeTest::testAvxConvenience()
{
    // Test _mm256_minidx_ps
    union {
        __m256 testV;
        float testF[8];
    };
    float minVal;
    for (unsigned i = 0; i < 8; ++i) {
        minVal = 6.0;
        testV = _mm256_set1_ps(5.0);
        testF[i] = 4.0;
        CPPUNIT_ASSERT_EQUAL(i, _mm256_minidx_ps(testV, &minVal));
        CPPUNIT_ASSERT_EQUAL(minVal, 4.0f);
    }
}
