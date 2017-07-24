#ifndef AVXCONVENIENCE_H
#define AVXCONVENIENCE_H

#include <immintrin.h>


/*
	AVX:    256 bit per register
	SSE:    128 bit per register
	float:   32 bit per value
*/
#define FLOATSPERVECTOR 8
#define BYTESPERVECTOR 32


static inline float reduce_add_ps(__m256 x) {
    /* ( x3+x7, x2+x6, x1+x5, x0+x4 ) */
    const __m128 x128 = _mm_add_ps(_mm256_extractf128_ps(x, 1), _mm256_castps256_ps128(x));
    /* ( -, -, x1+x3+x5+x7, x0+x2+x4+x6 ) */
    const __m128 x64 = _mm_add_ps(x128, _mm_movehl_ps(x128, x128));
    /* ( -, -, -, x0+x1+x2+x3+x4+x5+x6+x7 ) */
    const __m128 x32 = _mm_add_ss(x64, _mm_shuffle_ps(x64, x64, 0x55));
    /* Conversion to float is a no-op on x86-64 */
    return _mm_cvtss_f32(x32);
}

static inline float reduce_xor_ps(__m256 x) {
    /* ( x3+x7, x2+x6, x1+x5, x0+x4 ) */
    const __m128 x128 = _mm_xor_ps(_mm256_extractf128_ps(x, 1), _mm256_castps256_ps128(x));
    /* ( -, -, x1+x3+x5+x7, x0+x2+x4+x6 ) */
    const __m128 x64 = _mm_xor_ps(x128, _mm_movehl_ps(x128, x128));
    /* ( -, -, -, x0+x1+x2+x3+x4+x5+x6+x7 ) */
    const __m128 x32 = _mm_xor_ps(x64, _mm_shuffle_ps(x64, x64, 0x55));
    /* Conversion to float is a no-op on x86-64 */
    return _mm_cvtss_f32(x32);
}

static inline float _mm_reduce_xor_ps(__m128 x) {
    /* ( -, -, x1+x3+x5+x7, x0+x2+x4+x6 ) */
    const __m128 x64 = _mm_xor_ps(x, _mm_movehl_ps(x, x));
    /* ( -, -, -, x0+x1+x2+x3+x4+x5+x6+x7 ) */
    const __m128 x32 = _mm_xor_ps(x64, _mm_shuffle_ps(x64, x64, 0x55));
    /* Conversion to float is a no-op on x86-64 */
    return _mm_cvtss_f32(x32);
}




static inline float _mm_reduce_add_ps(__m128 x) {
    /* ( -, -, x1+x3+x5+x7, x0+x2+x4+x6 ) */
    const __m128 x64 = _mm_add_ps(x, _mm_movehl_ps(x, x));
    /* ( -, -, -, x0+x1+x2+x3+x4+x5+x6+x7 ) */
    const __m128 x32 = _mm_add_ss(x64, _mm_shuffle_ps(x64, x64, 0x55));
    /* Conversion to float is a no-op on x86-64 */
    return _mm_cvtss_f32(x32);
}

static inline unsigned _mm_minidx_ps(__m128 x)
{
	const __m128 halfMinVec = _mm_min_ps(x, _mm_permute_ps(x, 0b01001110));
	const __m128 minVec = _mm_min_ps(halfMinVec, _mm_permute_ps(halfMinVec, 0b10110001));
	const __m128 mask = _mm_cmpeq_ps(x, minVec);
	return __tzcnt_u32(_mm_movemask_ps(mask));
}

static inline unsigned _mm256_minidx_ps(__m256 x, float *minVal) {
	//Lazy version

	const unsigned a = _mm_minidx_ps(_mm256_extractf128_ps(x, 0));
	const unsigned b = _mm_minidx_ps(_mm256_extractf128_ps(x, 1))+4;

	if(x[a] < x[b]) {
		*minVal = x[a];
		return a;
	}//else
		*minVal = x[b];
		return b;
}


/** \brief Returns the index of the smallest element of x.
 * 
 * This is an extension to the _mm_minpos_epu16()-function,
 * which is the only available function of its kind that returns the position
 * of the smallest unsigned 16-bit integer in a given vector.
 * _mm256_minpos_epu8() utilizes it to find the smallest unsigned 8-bit integer
 * in vector x and returns the respective position.
 * 
 */
unsigned _mm256_minpos_epu8(__m256i x);

/*!
 * \brief Create a sub-vector-size child node by shifting the right-hand side bits.
 * \param x The vector containing left and right bits.
 * \param shift The number of bits to shift.
 * \return The right child node's bits.
 */
__m256i _mm256_subVectorShift_epu8(__m256i x, int shift);


#endif //AVXCONVENIENCE
