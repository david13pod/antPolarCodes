#ifndef PC_BITCONTAINER_H
#define PC_BITCONTAINER_H

#include <cstddef>
#include <set>

namespace PolarCode {

/*!
 * \brief The BitContainer skeleton-class
 *
 * As this project uses different bit formats in the coding algorithms,
 * a converting bit container is needed.
 */
class BitContainer {

protected:
	size_t mElementCount;///< The fixed number of bits stored in this container.
public:
	BitContainer();
	virtual ~BitContainer();

	/*!
	 * \brief Sets the number of bits to be stored in this container.
	 * \param newSize The new bit count.
	 */
	virtual void setSize(size_t newSize) = 0;

	/*!
	 * \brief Fill internal storage with given data.
	 *
	 * This function reads mElementCount/8 bytes from pData and inserts it
	 * into internal storage in the needed bit format.
	 *
	 * \sa insertPackedInformation()
	 * \param pData Any kind of pointer to memory location of packed bytes
	 */
	virtual void insertPacked(const void* pData) = 0;

	/*!
	 * \brief Insert bits into non-frozen locations.
	 * \sa insertPacked()
	 * \param pData Information bits.
	 * \param frozenBits Location indices of frozen bits.
	 */
	virtual void insertPackedInformation(const void *pData,
										   std::set<unsigned> &frozenBits) = 0;

	/*!
	 * \brief Write packed bits into pData.
	 *
	 * This function writes packed data into the preallocated memory location
	 * at pData. Note that pData must hold at least (mElementCount/8) bytes
	 * of allocated memory.
	 *
	 * \sa getPackedInformation()
	 * \param pData Pointer to destination memory for packed data.
	 */
	virtual void getPacked(void* pData) = 0;

	/*!
	 * \brief Write packed information bits into pData.
	 *
	 * This function iterates through internal storage and ignores frozen bits.
	 * Packed information bits are then written into pData. Note that
	 * at least (mElementCount-frozenBits.size())/8 bytes must be allocated
	 * to pData.
	 *
	 * \sa getPacked()
	 * \param pData Memory location for packed information data.
	 * \param frozenBits Indices of bits to ignore at information extraction.
	 */
	virtual void getPackedInformation(void* pData,
									  std::set<unsigned> &frozenBits) = 0;

	/*!
	 * \brief Set all frozen bits to zero.
	 * \param frozenBits The set of frozen bits.
	 */
	virtual void resetFrozenBits(std::set<unsigned> &frozenBits) = 0;
};

/*!
 * \brief A class that holds bits in floating point format of +/- zero.
 *
 * The polar coding calculations involving single precision floating point
 * arithmetic use a special bit format where the data bit is stored in the most
 * significant bit of the 32-bit word, which is the sign-bit. The absolute value
 * is fixed to zero and must not be other than zero as some functions use the
 * implicit sign()-function for XOR-operations, where ONLY the sign bit of the
 * other operand is to be changed according to the bit state.
 *
 * \sa CharContainer
 */
class FloatContainer : public BitContainer {
	float *mData;

public:
	FloatContainer();
	FloatContainer(size_t size);
	~FloatContainer();
	void setSize(size_t newSize);
	void insertPacked(const void* pData);
	void insertPackedInformation(const void *pData, std::set<unsigned> &frozenBits);
	void getPacked(void* pData);
	void getPackedInformation(void* pData, std::set<unsigned> &frozenBits);
	void resetFrozenBits(std::set<unsigned> &frozenBits);

	float* data();
};


/*!
 * \brief A class that holds bits in classic (0,1)-format.
 *
 * Polar Coding on eight-bit integers has its own challenges, different from
 * those of floating point arithmetic. Multiplication has to be avoided.
 * Instead the bits can be stored in classic (0,1)-format, which allows them to
 * be used as a mask-operand.
 */
class CharContainer : public BitContainer {
	char *mData;

public:
	CharContainer();
	CharContainer(size_t size);
	~CharContainer();
	void setSize(size_t newSize);
	void insertPacked(const void* pData);
	void insertPackedInformation(const void *pData, std::set<unsigned> &frozenBits);
	void getPacked(void* pData);
	void getPackedInformation(void* pData, std::set<unsigned> &frozenBits);
	void resetFrozenBits(std::set<unsigned> &frozenBits);

	char* data();
};

}//namespace PolarCode

#endif
