#pragma once

#include <cstdint>

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
PromotionState IsCompatibleReadWrite() noexcept;

/**
 * Tests if a read only transaction can be started.
 */
PromotionState IsCompatibleReadOnly() noexcept;

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
 * 
 * \note        The function may be called multiple times if the transaction needs to be restarted. Be
 *              careful about directly accessing captured variables.
 * 
 * \throw       TransactionError
 * 
 * \param   func    A callable object that represents the atomic function.
 */
template<class _Cl>
inline bool Atomic(_Cl func);

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
 * 
 * \note        The function may be called multiple times if the transaction needs to be restarted. Be
 *              careful about directly accessing captured variables.
 * 
 * \throw       TransactionError
 * 
 * \param   func    A callable object that represents the atomic function.
 */
template<class _Cl>
inline bool AtomicRead(_Cl func);

} // namespace transactional
namespace tr = transactional;
} // namespace nlane