/**
 * Copyright 2020 Lorenzo Rai
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software 
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. 
 */

/**
 * This file contains the public transactional memory interface.
 */

#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace nlane{
namespace transactional {

using Word = uint64_t;

// The bitmask of bits that must be 0 in a word aligned address
constexpr size_t kWordAlignMask{ alignof(Word) - 1u };

// The type used to represent version time stamps
using Version = uint64_t;

// The highest allowed version number before an overflow event
constexpr Version kMaxVersion{ std::numeric_limits<Version>::max() >> 2u };

/**
 * 
 */
class TransactionError : public std::runtime_error {
  private:
	bool recoverable_;

  public:
	TransactionError(const std::string& what, const bool retry);
	TransactionError(const char* what, const bool retry);
	TransactionError(const TransactionError& other) noexcept;

	virtual ~TransactionError();

	TransactionError& operator= (const TransactionError& other) noexcept;

    /**
     * Whether or not this error should cause a transaction to be retried or not.
     * 
     * \returns True if the transaction should be retried.
     */
	virtual bool shouldRetry() const noexcept;
};

namespace detail {

enum class PromotionState {
    NO_RUNNING,
    COMPATIBLE,
    INCOMPATIBLE,
};

/**
 * Tests if a read write transaction can be started.
 */
PromotionState IsReadWriteCompatible();

/**
 * Tests if a read only transaction can be started.
 */
PromotionState IsReadOnlyCompatible();

/**
 * Starts a read-write transaction.
 * 
 * \throw TransactionError
 */
void BeginReadWrite();

/**
 * Starts a read-only transaction.
 * 
 * \throw TransactionError
 */
void BeginReadOnly();

/**
 * Restarts a read-write transaction.
 * 
 * \throw TransactionError
 */
void RestartReadWrite();

/**
 * Restarts a read-only transaction.
 * 
 * \throw TransactionError
 */
void RestartReadOnly();

/**
 * Attempts to commit the currently running transaction.
 * 
 * If the commit is successful also terminates the transaction.
 * No call to end should be made if this function returns without throwing and exception.
 * 
 * \throw TransactionError
 */
void Commit();

/**
 * Terminates the currently running transaction without commiting.
 */
void End();

/**
 * \returns The address of the word containing the byte pointed to by addr
 */
inline void* WordAlignedAddress(void* addr);

} // namespace detail

/**
 * Initializes the thread local transaction engine.
 * Must be called before any other call to the thread local transaction engine.
 */
void ThreadInit();

/**
 * Atomically reads the word at specified address. 
 * Must be called within a transaction.
 * 
 * \throw TransactionError If an error occured. For example reading inconsitent state.
 *                         This error should be forwarded so that the transaction engine can restart.
 * 
 * \param address The address of the word.
 * \returns The content of the word pointed to by address.
 */
Word ReadWord(void* address);

/**
 * Atomically writes the word at specified address. 
 * Must be called within a transaction.
 * 
 * \throw TransactionError If an error occured. For example writing incositent state.
 *                         This error should be forwarded so that the transaction engine can restart.
 * 
 * \param address The address of the word.
 * \param data The data to be written.
 * \param mask A bitmask specifying which bits of the word should be written.
 */
void WriteWord(void* address, Word data, Word mask);

/**
 * Atomically reads the variable at specified address. 
 * Must be called within a transaction. 
 * 
 * \note Utility wrapper that hides translation to words. Internally calles ReadWord.
 * 
 * \param address A pointer to the requested data.
 * \returns The content of the variable pointed to by addr.
 */
template<typename _Ty, std::enable_if_t<!std::is_pointer_v<_Ty>, int> = 0>
inline _Ty Read(_Ty* addr);

/**
 * Atomically writes the variable at specified address. 
 * Must be called within a transaction.
 * 
 * \note Utility wrapper that hides translation to words. Internally calles WriteWord.
 * 
 * \throw TransactionError If an error occured. For example writing incositent state.
 *                         This error should be forwarded so that the transaction engine can restart.
 * 
 * \param address The pointer to the variable.
 * \param data The value to be written.
 */
template<typename _Ty, std::enable_if_t<!std::is_pointer_v<_Ty>, int> = 0>
inline void Write(_Ty* addr, _Ty data);

/**
 * Atomically reads the pointer at specified address. 
 * Must be called within a transaction. 
 * 
 * \note Utility wrapper that hides translation to words. Internally calles ReadWord.
 * 
 * \param address A pointer to the requested pointer.
 * \returns The content of the pointer pointed to by addr.
 */
template<typename _Ty>
inline _Ty* Read(_Ty** addr);

/**
 * Atomically writes the pointer at specified address. 
 * Must be called within a transaction.
 * 
 * \note Utility wrapper that hides translation to words. Internally calles WriteWord.
 * 
 * \throw TransactionError If an error occured. For example writing incositent state.
 *                         This error should be forwarded so that the transaction engine can restart.
 * 
 * \param address The pointer to the variable.
 * \param data The value to be written.
 */
template<typename _Ty>
inline void Write(_Ty** addr, _Ty* data);

/**
 * \brief       Atomically executes the passed function. Reads and writes are allowed.
 * 
 * \details     Atomically executes a function by wraping it inside a transaction. If no transaction is 
 *              currently running starts a new read-write transaction.
 *              If this function is called inside a compatible currently running transaction the atomic
 *              block becomes part of the encompasing transaction. I.e. it will only commit after the
 *              outter most transaction commits.
 *              If this function is called inside an incompatible transaction a no-restart TransactionError
 *              will be thrown.
 *              This function will always either return with the transaction successfully executed or throw
 *              an error.
 * 
 * \note        The function may be called multiple times if the transaction needs to be restarted. Be
 *              careful about directly accessing captured variables.
 * 
 * \throw       TransactionError
 * 
 * \param   func    A callable object that represents the atomic function.
 */
template<class _Cl>
inline void Atomic(_Cl func);

/**
 * \brief       Atomically executes the passed function. Only reads are allowed.
 * 
 * \details     Atomically executes a function by wraping it inside a transaction. If no transaction is 
 *              currently running starts a new read-only transaction.
 *              If this function is called inside a compatible currently running transaction the atomic
 *              block becomes part of the encompasing transaction. I.e. it will only commit after the
 *              outter most transaction commits.
 *              If this function is called inside an incompatible transaction a no-restart TransactionError
 *              will be thrown.
 *              This function will always either return with the transaction successfully executed or throw
 *              an error.
 * 
 * \note        The function may be called multiple times if the transaction needs to be restarted. Be
 *              careful about directly accessing captured variables.
 * 
 * \throw       TransactionError
 * 
 * \param   func    A callable object that represents the atomic function.
 */
template<class _Cl>
inline void AtomicRead(_Cl func);


//
// Inline function definitions
//

namespace detail {

inline void* WordAlignedAddress(void* addr) {
	return reinterpret_cast<void*>(reinterpret_cast<size_t>(addr) & ~kWordAlignMask);
}
} // namespace detail

template<>
inline uint8_t Read<uint8_t>(uint8_t* addr) {
	constexpr size_t kShift[]{ 0, 8, 16, 24, 32, 40, 48, 56 };

	const uint8_t index{ static_cast<uint8_t>(reinterpret_cast<size_t>(addr) & 0b111u) };
	Word data{ ReadWord(detail::WordAlignedAddress(addr)) };
	return static_cast<uint8_t>(data >> kShift[index]);
}

template<>
inline void Write<uint8_t>(uint8_t* addr, uint8_t data) {
	constexpr Word kMask[]{ 0x00000000000000FFu, 0x000000000000FF00u, 0x0000000000FF0000u, 0x00000000FF000000u,
		0x000000FF00000000u, 0x0000FF0000000000u, 0x00FF000000000000u, 0xFF00000000000000u };
	constexpr size_t kShift[]{ 0, 8, 16, 24, 32, 40, 48, 56 };

	const uint8_t index{ static_cast<uint8_t>(reinterpret_cast<size_t>(addr) & 0b111u) };
	Word d{ static_cast<Word>(data) << kShift[index] };
	WriteWord(detail::WordAlignedAddress(addr), d, kMask[index]);
}

template<>
inline int8_t Read<int8_t>(int8_t* addr) {
	return static_cast<int8_t>(Read<uint8_t>(reinterpret_cast<uint8_t*>(addr)));
}

template<>
inline void Write<int8_t>(int8_t* addr, int8_t data) {
	Write<uint8_t>(reinterpret_cast<uint8_t*>(addr), static_cast<uint8_t>(data));
}

template<>
inline uint16_t Read<uint16_t>(uint16_t* addr) {
	constexpr size_t kShift[]{ 0, 16, 32, 48 };

	const uint8_t index{ static_cast<uint8_t>((reinterpret_cast<size_t>(addr) & 0b110u) >> 1u) };
	Word data{ ReadWord(detail::WordAlignedAddress(addr)) };
	return static_cast<uint16_t>(data >> kShift[index]);
}

template<>
inline void Write<uint16_t>(uint16_t* addr, uint16_t data) {
	constexpr Word kMask[]{ 0x000000000000FFFFu, 0x00000000FFFF0000u, 0x0000FFFF00000000u, 0xFFFF000000000000u };
	constexpr size_t kShift[]{ 0, 16, 32, 48 };

	const uint8_t index{ static_cast<uint8_t>((reinterpret_cast<size_t>(addr) & 0b110u) >> 1u) };
	Word d{ static_cast<Word>(data) << kShift[index] };
	WriteWord(detail::WordAlignedAddress(addr), d, kMask[index]);
}

template<>
inline int16_t Read<int16_t>(int16_t* addr) {
	return static_cast<int16_t>(Read<uint16_t>(reinterpret_cast<uint16_t*>(addr)));
}

template<>
inline void Write<int16_t>(int16_t* addr, int16_t data) {
	Write<uint16_t>(reinterpret_cast<uint16_t*>(addr), static_cast<uint16_t>(data));
}

template<>
inline uint32_t Read<uint32_t>(uint32_t* addr) {
	constexpr size_t kShift[]{ 0, 32 };

	const uint8_t index{ static_cast<uint8_t>(reinterpret_cast<size_t>(addr) & 4u) != 0u };
	Word data{ ReadWord(detail::WordAlignedAddress(addr)) };
	return static_cast<uint32_t>(data >> kShift[index]);
}

template<>
inline void Write<uint32_t>(uint32_t* addr, uint32_t data) {
	constexpr Word kMask[]{ 0x00000000FFFFFFFFu, 0xFFFFFFFF00000000 };
	constexpr size_t kShift[]{ 0, 32 };

	const uint8_t index{ static_cast<uint8_t>(reinterpret_cast<size_t>(addr) & 4u) != 0u };
	Word d{ static_cast<Word>(data) << kShift[index] };
	WriteWord(detail::WordAlignedAddress(addr), d, kMask[index]);
}

template<>
inline int32_t Read<int32_t>(int32_t* addr) {
	return static_cast<int32_t>(Read<uint32_t>(reinterpret_cast<uint32_t*>(addr)));
}

template<>
inline void Write<int32_t>(int32_t* addr, int32_t data) {
	Write<uint32_t>(reinterpret_cast<uint32_t*>(addr), static_cast<uint32_t>(data));
}

template<>
inline uint64_t Read<uint64_t>(uint64_t* addr) {
	return ReadWord(addr);
}

template<>
inline void Write<uint64_t>(uint64_t* addr, uint64_t data) {
	return WriteWord(addr, data, ~static_cast<uint64_t>(0));
}

template<>
inline int64_t Read<int64_t>(int64_t* addr) {
	return ReadWord(addr);
}

template<>
inline void Write<int64_t>(int64_t* addr, int64_t data) {
	return WriteWord(addr, data, ~static_cast<uint64_t>(0));
}

template<>
inline float Read<float>(float* addr) {
	uint32_t data{ Read<uint32_t>(reinterpret_cast<uint32_t*>(addr)) };
	return *reinterpret_cast<float*>(&data);
}

template<>
inline void Write<float>(float* addr, float data) {
	Write<uint32_t>(reinterpret_cast<uint32_t*>(addr), *reinterpret_cast<uint32_t*>(&data));
}

template<>
inline double Read<double>(double* addr) {
	uint64_t data{ Read<uint64_t>(reinterpret_cast<uint64_t*>(addr)) };
	return *reinterpret_cast<double*>(&data);
}

template<>
inline void Write<double>(double* addr, double data) {
	Write<uint64_t>(reinterpret_cast<uint64_t*>(addr), *reinterpret_cast<uint64_t*>(&data));
}

template<class _Cl>
inline _Cl* Read<_Cl*>(_Cl** addr) {
	return reinterpret_cast<_Cl*>(Read<size_t>(reinterpret_cast<size_t*>(addr)));
}

template<class _Cl>
inline void Write<_Cl*>(_Cl** addr, _Cl* data) {
	Write<size_t>(reinterpret_cast<size_t*>(addr), reinterpret_cast<size_t>(data));
}

template<class _Cl>
inline void Atomic(_Cl func) {
    detail::PromotionState state{ detail::IsReadWriteCompatible() };
	if (state != detail::PromotionState::NO_RUNNING) {
        if(state == detail::PromotionState::COMPATIBLE) {
		    func();
            return;
        } else {
            throw TransactionError{ "Cannot embed read-write transaction inside read-only transaction", false };
        }
	}
	
	while (true) {
		try {
			detail::BeginReadWrite();

			func();

			detail::Commit();
			return;
		}
		catch (TransactionError& err) {
			if (!err.shouldRetry()) {
				detail::End();
				throw;
			}
		}
		catch (...) {
			detail::End();
			throw;
		}
	}
}

template<class _Cl>
inline void AtomicRead(_Cl func) {
    detail::PromotionState state{ detail::IsReadOnlyCompatible() };
	if (state != detail::PromotionState::NO_RUNNING) {
        if(state == detail::PromotionState::COMPATIBLE) {
		    func();
            return;
        } else {
            throw TransactionError{ "Read only transaction is for some reason incompatible. This should never happen.", false };
        }
	}
	
	while (true) {
		try {
			detail::BeginReadOnly();

			func();

			detail::Commit();
			return;
		}
		catch (TransactionError& err) {
			if (!err.shouldRetry()) {
				detail::End();
				throw;
			}
		}
		catch (...) {
			detail::End();
			throw;
		}
	}
}

} // namespace transactional
namespace tr = transactional;
} // namespace nlane