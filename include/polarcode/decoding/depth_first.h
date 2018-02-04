#ifndef PC_DEC_DEPTHFIRST_H
#define PC_DEC_DEPTHFIRST_H

#include <polarcode/decoding/decoder.h>
#include <polarcode/decoding/avx_float.h>
#include <polarcode/datapool.txx>
#include <vector>
#include <queue>

namespace PolarCode {
namespace Decoding {

namespace DepthFirstObjects {
typedef DataPool<float, 32> datapool_t;
typedef Block<float> block_t;


class Node;

struct DecoderHint {
	Node *node;
	float reliability;
};

inline bool compareHints(DecoderHint &a, DecoderHint &b) {
	return a.reliability < b.reliability;
}

struct Configuration {
	int depth;
	float parentMetric;
	std::vector<DecoderHint> nodeList;
	std::vector<int> nodeOptions;
	std::string output;
};

class compareConfigMetrics {
public:
	bool operator()(Configuration &a, Configuration &b) {
		return a.parentMetric < b.parentMetric;
	}
};

class Manager {
	std::vector<Node*> mNodeList;
	Node *xmRootNode;
//	std::priority_queue<Configuration, std::vector<Configuration>, compareConfigMetrics> mConfigList;
	std::queue<Configuration> mConfigList;

	Configuration mBestConfig;
	float mBestMetric;
	bool firstRun;

public:
	Manager();
	~Manager();

	/*!
	 * \brief Save a deciding node
	 * This function is called once on node creation, if the calling node makes
	 * decoding decisions (R1, Rep, SPC; not R0, RR).
	 * After decoding a frame, all decisice nodes can be asked about their
	 * weakest reliability.
	 */
	void pushDecoder(Node *);

	void setRootNode(Node *);

	/*!
	 * \brief First decoding run
	 */
	void decode();

	/*!
	 * \brief Change the weakest decision and decode again.
	 * If the output of the first decoding run is erroneous, call this function
	 * to change the weakest decoders' decision and decode all subsequent bits.
	 * This function asks each partial decoder in mNodeList about its
	 * reliability, then calls the weakest to change the decision and starts a
	 * partial decoding run, where calculations are re-evaluated at the specific
	 * node and the following ones, but not before it.
	 */
	void decodeNext();

	/*!
	 * \brief Fall back to best configuration seen so far.
	 */
	void decodeBestConfig();

};

class Node {
protected:
	unsigned mBlockLength;

	datapool_t *xmDataPool;
	Manager *xmManager;

	block_t *mLlr, *mBit;
	float *mInput, *mOutput;

	float mReliability;
	int mOptionCount;
	int mOption;

public:
	Node();
	Node(Node *other);
	Node(unsigned blockLength, datapool_t *pool, Manager *manager);
	virtual ~Node();

	void setInput(float *);
	virtual void setOutput(float *);

	unsigned blockLength();
	float* input();
	float* output();

	virtual void decode();

	float reliability();
	int optionCount();
	void setOption(int);
};

class RateRNode : public Node {
protected:
	Node *mLeft,    ///< Left child node
		 *mRight;   ///< Right child node
	block_t *mLeftLlr, *mRightLlr;

public:
	RateRNode();

	/*!
	 * \brief Create a decoder node.
	 * \param frozenBits The set of frozen bits for this code.
	 * \param parent The parent node to copy all information from.
	 */
	RateRNode(const std::vector<unsigned> &frozenBits, Node *parent);
	~RateRNode();
	void setOutput(float *);
	void decode();
};

class ShortRateRNode : public RateRNode {
	block_t *mLeftBits, *mRightBits;

public:
	/*!
	 * \brief Create a decoder node.
	 * \param frozenBits The set of frozen bits for this code.
	 * \param parent The parent node to copy all information from.
	 */
	ShortRateRNode(const std::vector<unsigned> &frozenBits, Node *parent);
	~ShortRateRNode();
	void setOutput(float *);
	void decode();
};

class RateZeroDecoder : public Node {
public:
	RateZeroDecoder(Node *parent);
	~RateZeroDecoder();
	void decode();
};

class RateOneDecoder : public Node {
	unsigned *mFlipIndices;
	block_t *mTempBlock;
	void findWeakLlrs();

public:
	RateOneDecoder(Node *parent);
	~RateOneDecoder();
	void decode();
};

class RepetitionDecoder : public Node {
public:
	RepetitionDecoder(Node *parent);
	~RepetitionDecoder();
	void decode();
};

class SpcDecoder : public Node {
	unsigned *mFlipIndices;
	block_t *mTempBlock;
	void findWeakLlrs();

public:
	SpcDecoder(Node *parent);
	~SpcDecoder();
	void decode();
};

Node* createDecoder(const std::vector<unsigned> &frozenBits, Node* parent);

}// namespace DepthFirstObjects

class DepthFirst : public Decoder {
	size_t mTrialLimit;
	DepthFirstObjects::Node *mNodeBase, *mRootNode;
	DepthFirstObjects::datapool_t *mDataPool;
	DepthFirstObjects::Manager *mManager;

	void clear();

public:
	DepthFirst(size_t blockLength, size_t trialLimit, const std::vector<unsigned> &frozenBits);
	~DepthFirst();

	bool decode();
	void initialize(size_t blockLength, const std::vector<unsigned> &frozenBits);
};


}// namespace Decoding
}// namespace PolarCode

#endif// PC_DEC_DEPTHFIRST_H
