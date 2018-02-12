#include <polarcode/decoding/fastssc_avx_float.h>
#include <polarcode/encoding/butterfly_avx2_packed.h>
#include <polarcode/polarcode.h>

#include <string>
#include <iostream>

#include <cstring> //for memset
#include <cmath>

namespace PolarCode {
namespace Decoding {

namespace FastSscAvx {

inline void memFloatFill(float *dst, float value, const size_t blockLength) {
	if(blockLength>=8) {
		const __m256 vec = _mm256_set1_ps(value);
		for(unsigned i=0; i<blockLength; i+=8) {
			_mm256_store_ps(dst+i, vec);
		}
	} else {
		for(unsigned i=0; i<blockLength; i++) {
			dst[i] = value;
		}
	}
}

Node::Node()
	: mBlockLength(0)
	, xmDataPool(nullptr)
	, mLlr(nullptr)
	, mBit(nullptr)
	, mInput(nullptr)
	, mOutput(nullptr)
{
}

Node::Node(Node *other)
	: mBlockLength(other->mBlockLength)
	, xmDataPool(other->xmDataPool)
	, mLlr(xmDataPool->allocate(mBlockLength))
	, mBit(xmDataPool->allocate(mBlockLength))
	, mInput(other->mInput)
	, mOutput(other->mOutput)
{
}

Node::Node(size_t blockLength, datapool_t *pool)
	: mBlockLength(blockLength)
	, xmDataPool(pool)
	, mLlr(pool->allocate(blockLength))
	, mBit(pool->allocate(blockLength))
	, mInput(mLlr->data)
	, mOutput(mBit->data)
{
}

Node::~Node() {
	if(mLlr != nullptr) xmDataPool->release(mLlr);
	if(mBit != nullptr) xmDataPool->release(mBit);
}

void Node::decode() {
}

void Node::setInput(float *input) {
	mInput = input;
}

void Node::setOutput(float *output) {
	mOutput = output;
}

unsigned Node::blockLength() {
	return mBlockLength;
}

datapool_t* Node::pool() {
	return xmDataPool;
}

float* Node::input() {
	return mInput;
}

float* Node::output() {
	return mOutput;
}


/*************
 * RateRNode
 * ***********/

RateRNode::RateRNode(const std::vector<unsigned> &frozenBits, Node *parent, ChildCreationFlags flags)
	: Node(parent)
{
	mBlockLength /= 2;

	std::vector<unsigned> leftFrozenBits, rightFrozenBits;
	splitFrozenBits(frozenBits, mBlockLength, leftFrozenBits, rightFrozenBits);

	if(flags & NO_LEFT) {
		mLeft = new Node();
	} else {
		mLeft = createDecoder(leftFrozenBits, this);
	}

	if(flags & NO_RIGHT) {
		mRight = new Node();
	} else {
		mRight = createDecoder(rightFrozenBits, this);
	}

	mLeftLlr  = xmDataPool->allocate(mBlockLength);
	mRightLlr = xmDataPool->allocate(mBlockLength);

	mLeft->setInput(mLeftLlr->data);
	mRight->setInput(mRightLlr->data);

	mLeft->setOutput(mOutput);
	mRight->setOutput(mOutput + mBlockLength);
}

RateRNode::~RateRNode() {
	delete mLeft;
	delete mRight;
	xmDataPool->release(mLeftLlr);
	xmDataPool->release(mRightLlr);
}

void RateRNode::setOutput(float *output) {
	mOutput = output;
	mLeft->setOutput(mOutput);
	mRight->setOutput(mOutput + mBlockLength);
}

void RateRNode::decode() {
	F_function(mInput, mLeftLlr->data, mBlockLength);
	mLeft->decode();
	G_function(mInput, mRightLlr->data, mOutput, mBlockLength);
	mRight->decode();
	CombineSoft(mOutput, mBlockLength);
}

/*************
 * ShortRateRNode
 * ***********/

ShortRateRNode::ShortRateRNode(const std::vector<unsigned> &frozenBits, Node *parent)
	: RateRNode(frozenBits, parent)
	, mLeftBits(xmDataPool->allocate(8))
	, mRightBits(xmDataPool->allocate(8))
{
	mLeft->setOutput(mLeftBits->data);
	mRight->setOutput(mRightBits->data);
}

ShortRateRNode::~ShortRateRNode() {
	xmDataPool->release(mLeftBits);
	xmDataPool->release(mRightBits);
}

void ShortRateRNode::setOutput(float *output) {
	mOutput = output;
}

void ShortRateRNode::decode() {
	F_function(mInput, mLeftLlr->data, mBlockLength);
	mLeft->decode();
	G_function(mInput, mRightLlr->data, mLeftBits->data, mBlockLength);
	mRight->decode();
	CombineSoftBitsShort(mLeftBits->data, mRightBits->data, mOutput, mBlockLength);
}

/*************
 * ROneNode
 * ***********/

ROneNode::ROneNode(const std::vector<unsigned> &frozenBits, Node *parent)
	: RateRNode(frozenBits, parent, NO_RIGHT) {
}

ROneNode::~ROneNode() {
}

void ROneNode::decode() {
	F_function(mInput, mLeftLlr->data, mBlockLength);
	mLeft->decode();
	rightDecode();
}

void ROneNode::rightDecode() {
	for(unsigned i = 0; i < mBlockLength; i+=8) {
		__m256 Llr_l = _mm256_load_ps(mInput+i);
		__m256 Llr_r = _mm256_load_ps(mInput+mBlockLength+i);
		__m256 Bits = _mm256_load_ps(mOutput+i);
		__m256 HBits = hardDecode(Bits);

		__m256 Llr_o = _mm256_xor_ps(Llr_l, HBits);//G-function
		Llr_o = _mm256_add_ps(Llr_o, Llr_r);//G-function
		/*nop*/ //Rate 1 decoder
		_mm256_store_ps(mOutput+mBlockLength+i, Llr_o);//Right bit
		F_function_calc(Bits, Llr_o, mOutput+i);//Combine left bit
	}
}

/*************
 * ZeroRNode
 * ***********/

ZeroRNode::ZeroRNode(const std::vector<unsigned> &frozenBits, Node *parent)
	: RateRNode(frozenBits, parent, NO_LEFT) {
}

ZeroRNode::~ZeroRNode() {
}

void ZeroRNode::decode() {
	G_function_0R(mInput, mRightLlr->data, mBlockLength);
	mRight->decode();
	Combine_0R(mOutput, mBlockLength);
}

/*************
 * RateZeroDecoder
 * ***********/

RateZeroDecoder::RateZeroDecoder(Node *parent)
	: Node(parent) {
}

RateZeroDecoder::~RateZeroDecoder() {
}

void RateZeroDecoder::decode() {
	memFloatFill(mOutput, INFINITY, mBlockLength);
}

/*************
 * RateOneDecoder
 * ***********/

RateOneDecoder::RateOneDecoder(Node *parent)
	: Node(parent) {
}

RateOneDecoder::~RateOneDecoder() {
}

void RateOneDecoder::decode() {
	for(unsigned i=0; i<mBlockLength; i+=8) {
		__m256 llr = _mm256_load_ps(mInput+i);
		_mm256_store_ps(mOutput+i, llr);
	}
}

/*************
 * RepetitionDecoder
 * ***********/

RepetitionDecoder::RepetitionDecoder(Node *parent)
	: Node(parent) {
}

RepetitionDecoder::~RepetitionDecoder() {
}

void RepetitionDecoder::decode() {
	__m256 LlrSum = _mm256_setzero_ps();

	RepetitionPrepare(mInput, mBlockLength);

	// Accumulate vectors
	for(unsigned i=0; i<mBlockLength; i+=8) {
		LlrSum = _mm256_add_ps(LlrSum, _mm256_load_ps(mInput+i));
	}

	// Get final sum and save decoding result
	float Bits = reduce_add_ps(LlrSum);
	memFloatFill(mOutput, Bits, mBlockLength);
}

/*************
 * SpcDecoder
 * ***********/

SpcDecoder::SpcDecoder(Node *parent)
	: Node(parent) {
	mTempBlock = xmDataPool->allocate(mBlockLength);
	mTempBlockPtr = mTempBlock->data;
}

SpcDecoder::~SpcDecoder() {
	xmDataPool->release(mTempBlock);
}

void SpcDecoder::decode() {
	const __m256 sgnMask = _mm256_set1_ps(-0.0);
	__m256 parVec = _mm256_setzero_ps();
	unsigned minIdx = 0;
	float testAbs, minAbs = INFINITY;

	SpcPrepare(mInput, mBlockLength);

	for(unsigned i=0; i<mBlockLength; i+=8) {
		__m256 vecIn = _mm256_load_ps(mInput+i);
		_mm256_store_ps(mOutput+i, vecIn);

		parVec = _mm256_xor_ps(parVec, vecIn);

		__m256 abs = _mm256_andnot_ps(sgnMask, vecIn);
		unsigned vecMin = _mm256_minidx_ps(abs, &testAbs);
		if(testAbs < minAbs) {
			minIdx = vecMin+i;
			minAbs = testAbs;
		}
	}

	// Flip least reliable bit, if neccessary
	union {
		float fParity;
		unsigned int iParity;
	};
	fParity = reduce_xor_ps(parVec);
	iParity &= 0x80000000;
	reinterpret_cast<unsigned int*>(mOutput)[minIdx] ^= iParity;
}

/*************
 * ZeroSpcDecoder
 * ***********/

ZeroSpcDecoder::ZeroSpcDecoder(Node *parent)
	: Node(parent) {
	mTempBlock = xmDataPool->allocate(mBlockLength);
	mTempBlockPtr = mTempBlock->data;
}

ZeroSpcDecoder::~ZeroSpcDecoder() {
	xmDataPool->release(mTempBlock);
}

void ZeroSpcDecoder::decode() {
	const __m256 sgnMask = _mm256_set1_ps(-0.0);
	const size_t subBlockLength = mBlockLength/2;
	__m256 parVec = _mm256_setzero_ps();
	unsigned minIdx = 0;
	float testAbs, minAbs = INFINITY;

	//Check parity equation
	for(unsigned i=0; i<subBlockLength; i+=8) {
		//G-function with only frozen bits
		__m256 left = _mm256_load_ps(mInput+i);
		__m256 right = _mm256_load_ps(mInput+subBlockLength+i);
		__m256 llr = _mm256_add_ps(left, right);

		//Save output
		_mm256_store_ps(mOutput+i, right);
		_mm256_store_ps(mOutput+subBlockLength+i, right);

		//Update parity counter
		parVec = _mm256_xor_ps(parVec, llr);

		// Only search for minimum if there is a chance for smaller absolute value
		if(minAbs > 0) {
			__m256 abs = _mm256_andnot_ps(sgnMask, llr);
			unsigned vecMin = _mm256_minidx_ps(abs, &testAbs);
			if(testAbs < minAbs) {
				minIdx = vecMin+i;
				minAbs = testAbs;
			}
		}
	}

	// Flip least reliable bit, if neccessary
	union {
		float fParity;
		unsigned int iParity;
	};
	fParity = reduce_xor_ps(parVec);
	iParity &= 0x80000000;
	unsigned *iOutput = reinterpret_cast<unsigned*>(mOutput);
	iOutput[minIdx] ^= iParity;
	iOutput[minIdx+subBlockLength] ^= iParity;
}

// End of decoder definitions

Node* createDecoder(const std::vector<unsigned> &frozenBits, Node* parent) {
	size_t blockLength = parent->blockLength();
	size_t frozenBitCount = frozenBits.size();

	// Begin with the two most simple codes:
	if(frozenBitCount == blockLength) {
		return new RateZeroDecoder(parent);
	}
	if(frozenBitCount == 0) {
		return new RateOneDecoder(parent);
	}

	// Following are "one bit unlike the others" codes:
	if(frozenBitCount == (blockLength-1)) {
		return new RepetitionDecoder(parent);
	}
	if(frozenBitCount == 1) {
		return new SpcDecoder(parent);
	}

	// Fallback: No special code available, split into smaller subcodes
	if(blockLength <= 8) {
		return new ShortRateRNode(frozenBits, parent);
	} else {
		std::vector<unsigned> leftFrozenBits, rightFrozenBits;
		splitFrozenBits(frozenBits, blockLength/2, leftFrozenBits, rightFrozenBits);

		//Last case of optimization:
		//Common child node combination(s)
		if(leftFrozenBits.size() == blockLength/2 && rightFrozenBits.size() == 1) {
			return new ZeroSpcDecoder(parent);
		}


		//Minor optimization:
		//Right rate-1
		if(rightFrozenBits.size() == 0) {
			return new ROneNode(frozenBits, parent);
		}
		//Left rate-0
		if(leftFrozenBits.size() == blockLength/2) {
			return new ZeroRNode(frozenBits, parent);
		}

		return new RateRNode(frozenBits, parent);
	}
}

}// namespace FastSscAvx

FastSscAvxFloat::FastSscAvxFloat(size_t blockLength, const std::vector<unsigned> &frozenBits) {
	initialize(blockLength, frozenBits);
}

FastSscAvxFloat::~FastSscAvxFloat() {
	clear();
}

void FastSscAvxFloat::clear() {
	delete mRootNode;
	delete mNodeBase;
	delete mDataPool;
}

void FastSscAvxFloat::initialize(size_t blockLength, const std::vector<unsigned> &frozenBits) {
	if(blockLength == mBlockLength && frozenBits == mFrozenBits) {
		return;
	}
	if(mBlockLength != 0) {
		clear();
	}
	mBlockLength = blockLength;
	//mFrozenBits = frozenBits;
	mFrozenBits.assign(frozenBits.begin(), frozenBits.end());
	mDataPool = new DataPool<float, 32>();
	mNodeBase = new FastSscAvx::Node(mBlockLength, mDataPool);
	mRootNode = FastSscAvx::createDecoder(mFrozenBits, mNodeBase);
	mLlrContainer = new FloatContainer(mNodeBase->input(),  mBlockLength);
	mBitContainer = new FloatContainer(mNodeBase->output(), mBlockLength);
	mLlrContainer->setFrozenBits(mFrozenBits);
	mBitContainer->setFrozenBits(mFrozenBits);
	mOutputContainer = new unsigned char[(mBlockLength-mFrozenBits.size()+7)/8];
}

bool FastSscAvxFloat::decode() {
	mRootNode->decode();

	if(!mSystematic) {
		Encoding::Encoder* encoder = new Encoding::ButterflyAvx2Packed(mBlockLength);
		encoder->setSystematic(false);
		encoder->setCodeword(dynamic_cast<FloatContainer*>(mBitContainer)->data());
		encoder->encode();
		encoder->getEncodedData(dynamic_cast<FloatContainer*>(mBitContainer)->data());
		delete encoder;
	}
	mBitContainer->getPackedInformationBits(mOutputContainer);
	bool result = mErrorDetector->check(mOutputContainer, (mBlockLength-mFrozenBits.size()+7)/8);
	return result;
}


}// namespace Decoding
}// namespace PolarCode
