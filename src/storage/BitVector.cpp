#include <boost/container/flat_set.hpp>
#include <iostream>
#include <algorithm>

#include "src/storage/BitVector.h"
#include "src/exceptions/InvalidArgumentException.h"
#include "src/exceptions/OutOfRangeException.h"

#include "src/utility/OsDetection.h"
#include "src/utility/Hash.h"
#include "src/utility/macros.h"

namespace storm {
    namespace storage {

        BitVector::const_iterator::const_iterator(uint64_t const* dataPtr, uint_fast64_t startIndex, uint_fast64_t endIndex, bool setOnFirstBit) : dataPtr(dataPtr), endIndex(endIndex) {
            if (setOnFirstBit) {
                // Set the index of the first set bit in the vector.
                currentIndex = getNextSetIndex(dataPtr, startIndex, endIndex);
            } else {
                currentIndex = startIndex;
            }
        }

        BitVector::const_iterator::const_iterator(const_iterator const& other) : dataPtr(other.dataPtr), currentIndex(other.currentIndex), endIndex(other.endIndex) {
            // Intentionally left empty.
        }

        BitVector::const_iterator& BitVector::const_iterator::operator=(const_iterator const& other) {
            // Only assign contents if the source and target are not the same.
            if (this != &other) {
                dataPtr = other.dataPtr;
                currentIndex = other.currentIndex;
                endIndex = other.endIndex;
            }
            return *this;
        }

        BitVector::const_iterator& BitVector::const_iterator::operator++() {
            currentIndex = getNextSetIndex(dataPtr, ++currentIndex, endIndex);
            return *this;
        }

        BitVector::const_iterator& BitVector::const_iterator::operator+=(size_t n) {
            for(size_t i = 0; i < n; ++i) {
                currentIndex = getNextSetIndex(dataPtr, ++currentIndex, endIndex);
            }
            return *this;
        }

        uint_fast64_t BitVector::const_iterator::operator*() const {
            return currentIndex;
        }

        bool BitVector::const_iterator::operator!=(const_iterator const& other) const {
            return currentIndex != other.currentIndex;
        }

        bool BitVector::const_iterator::operator==(const_iterator const& other) const {
            return currentIndex == other.currentIndex;
        }

        BitVector::BitVector() : bitCount(0), buckets(nullptr) {
            // Intentionally left empty.
        }

        BitVector::BitVector(uint_fast64_t length, bool init) : bitCount(length) {
            // Compute the correct number of buckets needed to store the given number of bits.
            uint_fast64_t bucketCount = length >> 6;
            if ((length & mod64mask) != 0) {
                ++bucketCount;
            }

            // Initialize the storage with the required values.
            if (init) {
                buckets = new uint64_t[bucketCount];
                std::fill_n(buckets, bucketCount, -1ull);
                truncateLastBucket();
            } else {
                buckets = new uint64_t[bucketCount]();
            }
        }
        
        BitVector::~BitVector() {
            if (buckets != nullptr) {
                delete buckets;
            }
        }

        template<typename InputIterator>
        BitVector::BitVector(uint_fast64_t length, InputIterator begin, InputIterator end) : BitVector(length) {
            set(begin, end);
        }

        BitVector::BitVector(uint_fast64_t length, std::vector<uint_fast64_t> setEntries) : BitVector(length, setEntries.begin(), setEntries.end()) {
            // Intentionally left empty.
        }

        BitVector::BitVector(uint_fast64_t bucketCount, uint_fast64_t bitCount) : bitCount(bitCount) {
            STORM_LOG_ASSERT((bucketCount << 6) == bitCount, "Bit count does not match number of buckets.");
            buckets = new uint64_t[bucketCount]();
        }

        BitVector::BitVector(BitVector const& other) : bitCount(other.bitCount) {
            buckets = new uint64_t[other.bucketCount()];
            std::copy_n(other.buckets, other.bucketCount(), buckets);
        }

        BitVector& BitVector::operator=(BitVector const& other) {
            // Only perform the assignment if the source and target are not identical.
            if (this != &other) {
                bitCount = other.bitCount;
                buckets = new uint64_t[other.bucketCount()];
                std::copy_n(other.buckets, other.bucketCount(), buckets);
            }
            return *this;
        }

        bool BitVector::operator<(BitVector const& other) const {
            if (this->size() < other.size()) {
                return true;
            } else if (this->size() > other.size()) {
                return false;
            }

            uint64_t* first1 = this->buckets;
            uint64_t* last1 = this->buckets + this->bucketCount();
            uint64_t* first2 = other.buckets;

            for (; first1 != last1; ++first1, ++first2) {
                if (*first1 < *first2) {
                    return true;
                } else if (*first1 > *first2) {
                    return false;
                }
            }
            return false;
        }

        BitVector& BitVector::operator=(BitVector&& other) {
            // Only perform the assignment if the source and target are not identical.
            if (this != &other) {
                bitCount = other.bitCount;
                this->buckets = other.buckets;
                other.buckets = nullptr;
            }

            return *this;
        }

        bool BitVector::operator==(BitVector const& other) const {
            // If the lengths of the vectors do not match, they are considered unequal.
            if (this->bitCount != other.bitCount) return false;

            // If the lengths match, we compare the buckets one by one.
            return std::equal(this->buckets, this->buckets + this->bucketCount(), other.buckets);
        }

        bool BitVector::operator!=(BitVector const& other) const {
            return !(*this == other);
        }

        void BitVector::set(uint_fast64_t index, bool value) {
            STORM_LOG_ASSERT(index < bitCount, "Invalid call to BitVector::set: written index " << index << " out of bounds.");
            uint64_t bucket = index >> 6;

            uint64_t mask = 1ull << (63 - (index & mod64mask));
            if (value) {
                buckets[bucket] |= mask;
            } else {
                buckets[bucket] &= ~mask;
            }
        }

        template<typename InputIterator>
        void BitVector::set(InputIterator begin, InputIterator end) {
            for (InputIterator it = begin; it != end; ++it) {
                this->set(*it);
            }
        }

        bool BitVector::operator[](uint_fast64_t index) const {
            uint64_t bucket = index >> 6;
            uint64_t mask = 1ull << (63 - (index & mod64mask));
            return (this->buckets[bucket] & mask) == mask;
        }

        bool BitVector::get(uint_fast64_t index) const {
            STORM_LOG_ASSERT(index < bitCount, "Invalid call to BitVector::get: read index " << index << " out of bounds.");
            return (*this)[index];
        }

        void BitVector::resize(uint_fast64_t newLength, bool init) {
            if (newLength > bitCount) {
                uint_fast64_t newBucketCount = newLength >> 6;
                if ((newLength & mod64mask) != 0) {
                    ++newBucketCount;
                }

                if (newBucketCount > this->bucketCount()) {
                    uint64_t* newBuckets = new uint64_t[newBucketCount];
                    std::copy_n(buckets, this->bucketCount(), newBuckets);
                    if (init) {
                        newBuckets[this->bucketCount() - 1] |= ((1ull << (64 - (bitCount & mod64mask))) - 1ull);
                        std::fill_n(newBuckets, newBucketCount - this->bucketCount(), -1ull);
                    } else {
                        std::fill_n(newBuckets, newBucketCount - this->bucketCount(), 0);
                    }
                    delete buckets;
                    buckets = newBuckets;
                    bitCount = newLength;
                } else {
                    // If the underlying storage does not need to grow, we have to insert the missing bits.
                    if (init) {
                        buckets[this->bucketCount() - 1] |= ((1ull << (64 - (bitCount & mod64mask))) - 1ull);
                    }
                    bitCount = newLength;
                }
                truncateLastBucket();
            } else {
                uint_fast64_t newBucketCount = newLength >> 6;
                if ((newLength & mod64mask) != 0) {
                    ++newBucketCount;
                }

                // If the number of buckets needs to be reduced, we resize it now. Otherwise, we can just truncate the
                // last bucket.
                if (newBucketCount < this->bucketCount()) {
                    uint64_t* newBuckets = new uint64_t[newBucketCount];
                    std::copy_n(buckets, newBucketCount, newBuckets);
                    delete buckets;
                    buckets = newBuckets;
                    bitCount = newLength;
                }
                bitCount = newLength;
                truncateLastBucket();
            }
        }

        BitVector BitVector::operator&(BitVector const& other) const {
            STORM_LOG_ASSERT(bitCount == other.bitCount, "Length of the bit vectors does not match.");
            BitVector result(bitCount);
            std::transform(this->buckets, this->buckets + this->bucketCount(), other.buckets, result.buckets, [] (uint64_t const& a, uint64_t const& b) { return a & b; });
            return result;
        }

        BitVector& BitVector::operator&=(BitVector const& other) {
            STORM_LOG_ASSERT(bitCount == other.bitCount, "Length of the bit vectors does not match.");
            std::transform(this->buckets, this->buckets + this->bucketCount(), other.buckets, this->buckets, [] (uint64_t const& a, uint64_t const& b) { return a & b; });
            return *this;
        }

        BitVector BitVector::operator|(BitVector const& other) const {
            STORM_LOG_ASSERT(bitCount == other.bitCount, "Length of the bit vectors does not match.");
            BitVector result(bitCount);
            std::transform(this->buckets, this->buckets + this->bucketCount(), other.buckets, result.buckets, [] (uint64_t const& a, uint64_t const& b) { return a | b; });
            return result;
        }

        BitVector& BitVector::operator|=(BitVector const& other) {
            STORM_LOG_ASSERT(bitCount == other.bitCount, "Length of the bit vectors does not match.");
            std::transform(this->buckets, this->buckets + this->bucketCount(), other.buckets, this->buckets, [] (uint64_t const& a, uint64_t const& b) { return a | b; });
            return *this;
        }

        BitVector BitVector::operator^(BitVector const& other) const {
            STORM_LOG_ASSERT(bitCount == other.bitCount, "Length of the bit vectors does not match.");
            BitVector result(bitCount);
            std::transform(this->buckets, this->buckets + this->bucketCount(), other.buckets, result.buckets, [] (uint64_t const& a, uint64_t const& b) { return a ^ b; });
            result.truncateLastBucket();
            return result;
        }

        BitVector BitVector::operator%(BitVector const& filter) const {
            STORM_LOG_ASSERT(bitCount == filter.bitCount, "Length of the bit vectors does not match.");

            BitVector result(filter.getNumberOfSetBits());

            // If the current bit vector has not too many elements compared to the given bit vector we prefer iterating
            // over its elements.
            if (filter.getNumberOfSetBits() / 10 < this->getNumberOfSetBits()) {
                uint_fast64_t position = 0;
                for (auto bit : filter) {
                    if ((*this)[bit]) {
                        result.set(position);
                    }
                    ++position;
                }
            } else {
                // If the given bit vector had much fewer elements, we iterate over its elements and accept calling the
                // more costly operation getNumberOfSetBitsBeforeIndex on the current bit vector.
                for (auto bit : (*this)) {
                    if (filter[bit]) {
                        result.set(filter.getNumberOfSetBitsBeforeIndex(bit));
                    }
                }
            }

            return result;
        }

        BitVector BitVector::operator~() const {
            BitVector result(this->bitCount);
            std::transform(this->buckets, this->buckets + this->bucketCount(), result.buckets, [] (uint64_t const& a) { return ~a; });
            result.truncateLastBucket();
            return result;
        }

        void BitVector::complement() {
            std::transform(this->buckets, this->buckets + this->bucketCount(), this->buckets, [] (uint64_t const& a) { return ~a; });
            truncateLastBucket();
        }

        BitVector BitVector::implies(BitVector const& other) const {
            STORM_LOG_ASSERT(bitCount == other.bitCount, "Length of the bit vectors does not match.");

            BitVector result(bitCount);
            std::transform(this->buckets, this->buckets + this->bucketCount(), other.buckets, result.buckets, [] (uint64_t const& a, uint64_t const& b) { return (~a | b); });
            result.truncateLastBucket();
            return result;
        }

        bool BitVector::isSubsetOf(BitVector const& other) const {
            STORM_LOG_ASSERT(bitCount == other.bitCount, "Length of the bit vectors does not match.");

            uint64_t const* it1 = buckets;
            uint64_t const* ite1 = buckets + bucketCount();
            uint64_t const* it2 = other.buckets;
            
            for (; it1 != ite1; ++it1, ++it2) {
                if ((*it1 & *it2) != *it1) {
                    return false;
                }
            }
            return true;
        }

        bool BitVector::isDisjointFrom(BitVector const& other) const {
            STORM_LOG_ASSERT(bitCount == other.bitCount, "Length of the bit vectors does not match.");

            uint64_t const* it1 = buckets;
            uint64_t const* ite1 = buckets + bucketCount();
            uint64_t const* it2 = other.buckets;
            
            for (; it1 != ite1; ++it1, ++it2) {
                if ((*it1 & *it2) != 0) {
                    return false;
                }
            }
            return true;
        }

        bool BitVector::matches(uint_fast64_t bitIndex, BitVector const& other) const {
            STORM_LOG_ASSERT((bitIndex & mod64mask) == 0, "Bit index must be a multiple of 64.");
            STORM_LOG_ASSERT(other.size() <= this->size() - bitIndex, "Bit vector argument is too long.");

            // Compute the first bucket that needs to be checked and the number of buckets.
            uint64_t index = bitIndex >> 6;

            uint64_t const* first1 = buckets + index;
            uint64_t const* first2 = other.buckets;
            uint64_t const* last2 = other.buckets + other.bucketCount();

            for (; first2 != last2; ++first1, ++first2) {
                if (*first1 != *first2) {
                    return false;
                }
            }
            return true;
        }

        void BitVector::set(uint_fast64_t bitIndex, BitVector const& other) {
            STORM_LOG_ASSERT((bitIndex & mod64mask) == 0, "Bit index must be a multiple of 64.");
            STORM_LOG_ASSERT(other.size() <= this->size() - bitIndex, "Bit vector argument is too long.");

            // Compute the first bucket that needs to be checked and the number of buckets.
            uint64_t index = bitIndex >> 6;

            uint64_t* first1 = buckets + index;
            uint64_t const* first2 = other.buckets;
            uint64_t const* last2 = other.buckets + other.bucketCount();
            
            for (; first2 != last2; ++first1, ++first2) {
                *first1 = *first2;
            }
        }

        storm::storage::BitVector BitVector::get(uint_fast64_t bitIndex, uint_fast64_t numberOfBits) const {
            uint64_t numberOfBuckets = numberOfBits >> 6;
            uint64_t index = bitIndex >> 6;
            STORM_LOG_ASSERT(index + numberOfBuckets <= this->bucketCount(), "Argument is out-of-range.");

            storm::storage::BitVector result(numberOfBuckets, numberOfBits);
            std::copy(this->buckets + index, this->buckets + index + numberOfBuckets, result.buckets);
            result.truncateLastBucket();
            return result;
        }

        uint_fast64_t BitVector::getAsInt(uint_fast64_t bitIndex, uint_fast64_t numberOfBits) const {
            uint64_t bucket = bitIndex >> 6;
            uint64_t bitIndexInBucket = bitIndex & mod64mask;

            uint64_t mask;
            if (bitIndexInBucket == 0) {
                mask = -1ull;
            } else {
                mask = (1ull << (64 - bitIndexInBucket)) - 1ull;
            }

            if (bitIndexInBucket + numberOfBits < 64) {
                // If the value stops before the end of the bucket, we need to erase some lower bits.
                mask &= ~((1ull << (64 - (bitIndexInBucket + numberOfBits))) - 1ull);
                return (buckets[bucket] & mask) >> (64 - (bitIndexInBucket + numberOfBits));
            } else if (bitIndexInBucket + numberOfBits > 64) {
                // In this case, the integer "crosses" the bucket line.
                uint64_t result = (buckets[bucket] & mask);
                ++bucket;

                // Compute the remaining number of bits.
                numberOfBits -= (64 - bitIndexInBucket);

                // Shift the intermediate result to the right location.
                result <<= numberOfBits;

                // Strip away everything from the second bucket that is beyond the final index and add it to the
                // intermediate result.
                mask = ~((1ull << (64 - numberOfBits)) - 1ull);
                uint64_t lowerBits = buckets[bucket] & mask;
                result |= (lowerBits >> (64 - numberOfBits));

                return result;
            } else {
                // In this case, it suffices to take the current mask.
                return buckets[bucket] & mask;
            }
        }

        void BitVector::setFromInt(uint_fast64_t bitIndex, uint_fast64_t numberOfBits, uint64_t value) {
            STORM_LOG_ASSERT((value >> numberOfBits) == 0, "Integer value too large to fit in the given number of bits.");

            uint64_t bucket = bitIndex >> 6;
            uint64_t bitIndexInBucket = bitIndex & mod64mask;

            uint64_t mask;
            if (bitIndexInBucket == 0) {
                mask = -1ull;
            } else {
                mask = (1ull << (64 - bitIndexInBucket)) - 1ull;
            }

            if (bitIndexInBucket + numberOfBits < 64) {
                // If the value stops before the end of the bucket, we need to erase some lower bits.
                mask &= ~((1ull << (64 - (bitIndexInBucket + numberOfBits))) - 1ull);
                buckets[bucket] = (buckets[bucket] & ~mask) | (value << (64 - (bitIndexInBucket + numberOfBits)));
            } else if (bitIndexInBucket + numberOfBits > 64) {
                // Write the part of the value that falls into the first bucket.
                buckets[bucket] = (buckets[bucket] & ~mask) | (value >> (numberOfBits + (bitIndexInBucket - 64)));
                ++bucket;

                // Compute the remaining number of bits.
                numberOfBits -= (64 - bitIndexInBucket);

                // Shift the bits of the value such that the already set bits disappear.
                value <<= (64 - numberOfBits);

                // Put the remaining bits in their place.
                mask = ((1ull << (64 - numberOfBits)) - 1ull);
                buckets[bucket] = (buckets[bucket] & mask) | value;
            } else {
                buckets[bucket] = (buckets[bucket] & ~mask) | value;
            }
        }

        bool BitVector::empty() const {
            uint64_t* last = buckets + bucketCount();
            uint64_t* it = std::find(buckets, last, 0);
            return it != last;
        }

        bool BitVector::full() const {
            // Check that all buckets except the last one have all bits set.
            uint64_t* last = buckets + bucketCount() - 1;
            for (uint64_t const* it = buckets; it != last; ++it) {
                if (*it != -1ull) {
                    return false;
                }
            }

            // Now check whether the relevant bits are set in the last bucket.
            uint64_t mask = ~((1ull << (64 - (bitCount & mod64mask))) - 1ull);
            if ((*last & mask) != mask) {
                return false;
            }
            return true;
        }

        void BitVector::clear() {
            std::fill_n(buckets, this->bucketCount(), 0);
        }

        uint_fast64_t BitVector::getNumberOfSetBits() const {
            return getNumberOfSetBitsBeforeIndex(bitCount);
        }

        uint_fast64_t BitVector::getNumberOfSetBitsBeforeIndex(uint_fast64_t index) const {
            uint_fast64_t result = 0;

            // First, count all full buckets.
            uint_fast64_t bucket = index >> 6;
            for (uint_fast64_t i = 0; i < bucket; ++i) {
                // Check if we are using g++ or clang++ and, if so, use the built-in function
#if (defined (__GNUG__) || defined(__clang__))
                result += __builtin_popcountll(buckets[i]);
#elif defined WINDOWS
#include <nmmintrin.h>
                // If the target machine does not support SSE4, this will fail.
                result += _mm_popcnt_u64(bucketVector[i]);
#else
                uint_fast32_t cnt;
                uint_fast64_t bitset = buckets[i];
                for (cnt = 0; bitset; cnt++) {
                    bitset &= bitset - 1;
                }
                result += cnt;
#endif
            }

            // Now check if we have to count part of a bucket.
            uint64_t tmp = index & mod64mask;
            if (tmp != 0) {
                tmp = ~((1ll << (64 - (tmp & mod64mask))) - 1ll);
                tmp &= buckets[bucket];
                // Check if we are using g++ or clang++ and, if so, use the built-in function
#if (defined (__GNUG__) || defined(__clang__))
                result += __builtin_popcountll(tmp);
#else
                uint_fast32_t cnt;
                uint64_t bitset = tmp;
                for (cnt = 0; bitset; cnt++) {
                    bitset &= bitset - 1;
                }
                result += cnt;
#endif
            }

            return result;
        }
        
        std::vector<uint_fast64_t> BitVector::getNumberOfSetBitsBeforeIndices() const {
            std::vector<uint_fast64_t> bitsSetBeforeIndices;
            bitsSetBeforeIndices.reserve(this->size());
            uint_fast64_t lastIndex = 0;
            uint_fast64_t currentNumberOfSetBits = 0;
            for (auto index : *this) {
                while (lastIndex <= index) {
                    bitsSetBeforeIndices.push_back(currentNumberOfSetBits);
                    ++lastIndex;
                }
                ++currentNumberOfSetBits;
            }
            return bitsSetBeforeIndices;
        }

        size_t BitVector::size() const {
            return static_cast<size_t> (bitCount);
        }

        std::size_t BitVector::getSizeInBytes() const {
            return sizeof (*this) + sizeof (uint64_t) * bucketCount();
        }

        BitVector::const_iterator BitVector::begin() const {
            return const_iterator(buckets, 0, bitCount);
        }

        BitVector::const_iterator BitVector::end() const {
            return const_iterator(buckets, bitCount, bitCount, false);
        }

        uint_fast64_t BitVector::getNextSetIndex(uint_fast64_t startingIndex) const {
            return getNextSetIndex(buckets, startingIndex, bitCount);
        }

        uint_fast64_t BitVector::getNextSetIndex(uint64_t const* dataPtr, uint_fast64_t startingIndex, uint_fast64_t endIndex) {
            uint_fast8_t currentBitInByte = startingIndex & mod64mask;
            uint64_t const* bucketIt = dataPtr + (startingIndex >> 6);
            startingIndex = (startingIndex >> 6 << 6);

            uint64_t mask;
            if (currentBitInByte == 0) {
                mask = -1ull;
            } else {
                mask = (1ull << (64 - currentBitInByte)) - 1ull;
            }
            while (startingIndex < endIndex) {
                // Compute the remaining bucket content.
                uint64_t remainingInBucket = *bucketIt & mask;

                // Check if there is at least one bit in the remainder of the bucket that is set to true.
                if (remainingInBucket != 0) {
                    // As long as the current bit is not set, move the current bit.
                    while ((remainingInBucket & (1ull << (63 - currentBitInByte))) == 0) {
                        ++currentBitInByte;
                    }

                    // Only return the index of the set bit if we are still in the valid range.
                    if (startingIndex + currentBitInByte < endIndex) {
                        return startingIndex + currentBitInByte;
                    } else {
                        return endIndex;
                    }
                }

                // Advance to the next bucket.
                startingIndex += 64;
                ++bucketIt;
                mask = -1ull;
                currentBitInByte = 0;
            }
            return endIndex;
        }

        void BitVector::truncateLastBucket() {
            if ((bitCount & mod64mask) != 0) {
                buckets[bucketCount() - 1] &= ~((1ll << (64 - (bitCount & mod64mask))) - 1ll);
            }
        }

        size_t BitVector::bucketCount() const {
            return bitCount >> 6;
        }

        std::ostream& operator<<(std::ostream& out, BitVector const& bitvector) {
            out << "bit vector(" << bitvector.getNumberOfSetBits() << "/" << bitvector.bitCount << ") [";
            for (auto index : bitvector) {
                out << index << " ";
            }
            out << "]";

            return out;
        }

        std::size_t NonZeroBitVectorHash::operator()(storm::storage::BitVector const& bitvector) const {
            STORM_LOG_ASSERT(bitvector.size() > 0, "Cannot hash bit vector of zero size.");
            std::size_t result = 0;

            for (uint_fast64_t index = 0; index < bitvector.bucketCount(); ++index) {
                result ^= result << 3;
                result ^= result >> bitvector.getAsInt(index << 6, 5);
            }

            // Erase the last bit and add one to definitely make this hash value non-zero.
            result &= ~1ull;
            result += 1;

            return result;
        }

        // All necessary explicit template instantiations.
        template BitVector::BitVector(uint_fast64_t length, std::vector<uint_fast64_t>::iterator begin, std::vector<uint_fast64_t>::iterator end);
        template BitVector::BitVector(uint_fast64_t length, std::vector<uint_fast64_t>::const_iterator begin, std::vector<uint_fast64_t>::const_iterator end);
        template BitVector::BitVector(uint_fast64_t length, boost::container::flat_set<uint_fast64_t>::iterator begin, boost::container::flat_set<uint_fast64_t>::iterator end);
        template BitVector::BitVector(uint_fast64_t length, boost::container::flat_set<uint_fast64_t>::const_iterator begin, boost::container::flat_set<uint_fast64_t>::const_iterator end);
        template void BitVector::set(std::vector<uint_fast64_t>::iterator begin, std::vector<uint_fast64_t>::iterator end);
        template void BitVector::set(std::vector<uint_fast64_t>::const_iterator begin, std::vector<uint_fast64_t>::const_iterator end);
        template void BitVector::set(boost::container::flat_set<uint_fast64_t>::iterator begin, boost::container::flat_set<uint_fast64_t>::iterator end);
        template void BitVector::set(boost::container::flat_set<uint_fast64_t>::const_iterator begin, boost::container::flat_set<uint_fast64_t>::const_iterator end);
    }
}

namespace std {

    std::size_t hash<storm::storage::BitVector>::operator()(storm::storage::BitVector const& bv) const {
        return boost::hash_range(bv.bucketVector.begin(), bv.bucketVector.end());
    }
}