from libcpp.vector cimport vector
from libcpp cimport bool


cdef extern from "polarcode/encoding/encoder.h" namespace "PolarCode::Encoding":
    cdef cppclass Encoder:
        Encoder() except +
        void encode()
        void setInformation(void*)
        void getEncodedData(void*)
        size_t blockLength()
        void setSystematic(bool)
        bool isSystematic()
        vector[unsigned int] frozenBits()


cdef extern from "polarcode/encoding/butterfly_avx2_packed.h" namespace "PolarCode::Encoding":
    cdef cppclass ButterflyAvx2Packed(Encoder):
        ButterflyAvx2Packed(size_t, vector[unsigned int]) except +


cdef extern from "polarcode/encoding/butterfly_avx2_char.h" namespace "PolarCode::Encoding":
    cdef cppclass ButterflyAvx2Char(Encoder):
        ButterflyAvx2Char(size_t, vector[unsigned int]) except +


cdef extern from "polarcode/decoding/decoder.h" namespace "PolarCode::Decoding":
    cdef cppclass Decoder:
        Decoder() except +
        bool decode()
        #void setErrorDetection(ErrorDetection::Detector* pDetector)
        size_t blockLength()
        size_t infoLength()
        void setSignal(const float*)
        void setSignal(const char*)
        void getDecodedInformationBits(void*)
        vector[unsigned int] frozenBits()

    Decoder* makeDecoder(size_t, size_t, vector[unsigned int])


cdef extern from "polarcode/construction/constructor.h" namespace "PolarCode::Construction":
    vector[unsigned int] frozen_bits(const int blockLength, const int infoLength, const float designSNR)

