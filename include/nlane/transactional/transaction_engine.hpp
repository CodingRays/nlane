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
 * This file contains per thread objects that manage transactions.
 */

#pragma once

#include <cassert>
#include <chrono>
#include <thread>

#include "transactional.hpp"
#include "transaction_support.hpp"

namespace nlane::transactional::detail {

class TransactionEngine;

// The per thread transaction engine
extern thread_local TransactionEngine thread_engine;

// Returns the smallest multiple of align that is greater or equal to size
constexpr size_t AlignedSize(const size_t size, const size_t align) {
    assert((align && (align & (align - 1u)) == 0u));
    return (size + align - 1u) & ~(align - 1u);
}

/**
 * Datastructure used to keep track of transaction entries. Its a linear list
 * that only supports append, clear, search and iteration.
 * 
 * _Ty needs to be default constructible, have a key type Key defined,
 * the comparison operator with the key type as well as the assignment 
 * operator with the key type defined.
 */
template<class _Ty, size_t kNumEntries = 256>
class PooledList {
  public:
	static constexpr size_t kEntrySize{ AlignedSize(sizeof(_Ty), alignof(_Ty)) };
	static constexpr size_t kPoolSizeBytes{ AlignedSize(kEntrySize * kNumEntries, 64u) };

	using Key = typename _Ty::Key;

  private:
	void* main_pool_{ nullptr };
	size_t next_index_{ 0 };

    // Prevent unwanted sharing
	struct alignas(64) PoolPage {
		uint8_t data[kPoolSizeBytes];
	};

    // Sanity check
	static_assert(sizeof(PoolPage) == kPoolSizeBytes);

    // Returns the entry at specified index. Does not do any bounds checking.
	_Ty* GetEntry(size_t index);

  public:
    // Does not allocate memory. A call to Init is required before use.
	inline PooledList();
	inline ~PooledList();

	// Initializes the list
	inline void Init();

    // Creates a new entry and sets its key
	inline _Ty* Create(Key key);

    // Attempts to find a entry. If it cant creates a new one and sets its key
	inline _Ty* GetOrCreate(Key key);

    // Searches for a entry.
	inline _Ty* Get(Key key);

    // Returns true if the list contains an entry with specified key.
	inline bool Contains(Key key);

    // Clears the list
	inline void Clear();

    // Returns true if the list is empty
	inline bool Empty() const;

    // Returns the number of entrys currently in the list
	inline size_t GetSize() const;

	class Iterator {
	  public:
		using iterator_category = std::input_iterator_tag;
		using value_type = _Ty;
		using pointer = _Ty*;
		using reference = _Ty&;
		using difference_type = size_t;

	  private:
		PooledList* list_;
		size_t current_index_;

	  public:
		inline Iterator(PooledList& list, size_t start);

		inline Iterator(const Iterator& other);
		inline Iterator(Iterator&& other);

		inline Iterator& operator=(const Iterator& other);
		inline Iterator& operator=(Iterator&& other);

		inline bool operator==(const Iterator& other) const;
		inline bool operator!=(const Iterator& other) const;

		inline Iterator& operator++();
				
		inline reference operator*();
	};

	inline Iterator begin();
	inline Iterator end();
};

/**
 * A class implementing the Xoroshiro128++ pseudo random number generator.
 * See http://prng.di.unimi.it/ for more details.
 */
class Xoroshiro128pp {
  private:
	uint64_t s_[2];

	static inline uint64_t Rotl(const uint64_t x, int k);

  public:
	inline Xoroshiro128pp();
	inline Xoroshiro128pp(uint64_t s0, uint64_t s1);

	inline uint64_t Next();
    inline void Jump();
};

class ReadSetEntry {
  public:
	using Key = LockIndex;

  private:
	Key index_;
	Version old_version_;

  public:
	inline ReadSetEntry& operator=(const Key key);
	inline bool operator==(const Key other) const;

	inline void SetVersion(Version version) noexcept;

	inline Key GetIndex() const noexcept;
	inline Version GetVersion() const noexcept;
};

class WriteSetEntry {
  public:
	using Key = LockIndex;

  private:
	Key index_;

  public:
	inline WriteSetEntry& operator=(const Key key);
	inline bool operator==(const Key other) const;

	inline Key GetIndex() const noexcept;
};

class WriteData {
  public:
	using Key = size_t;

  private:
	Key  address_;
	Word data_;
	Word mask_;

  public:
	inline WriteData& operator=(const Key key);
	inline bool operator==(const Key other) const;

	inline void Set(Word data, Word mask);
	inline void Extend(Word data, Word mask);

	inline Key GetAddress() const noexcept;
	inline Word GetData() const noexcept;
	inline Word GetMask() const noexcept;
};

enum class State : uint32_t {
	NONE_mask				= 0,
	ALL_mask				= ~(static_cast<uint32_t>(0)),

	INITIALIZED_bit			= 0b0001,
	RUNNING_bit				= 0b0010,
	READ_ONLY_bit			= 0b0100,
	SINGLE_bit				= 0b1000,

	UNINITIALIZED			= 0x0,
	INITIALIZED				= INITIALIZED_bit,
	READ_WRITE_RUNNING		= INITIALIZED_bit | RUNNING_bit,
	READ_ONLY_RUNNING		= INITIALIZED_bit | RUNNING_bit | READ_ONLY_bit,
};

inline State operator|(const State a, const State b) noexcept {
	return static_cast<State>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline State operator&(const State a, const State b) noexcept {
	return static_cast<State>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline State operator^(const State a, const State b) noexcept {
	return static_cast<State>(static_cast<uint32_t>(a) ^ static_cast<uint32_t>(b));
}

class alignas(64) TransactionEngine {
  private:
	LockEntry* lock_table_;
	State state_{ State::UNINITIALIZED };
	Version version_{ 0u };

	std::atomic<Version> cm_ts_{ std::numeric_limits<Version>::max() };
	uint16_t cm_backoff_{ 0u };

	PooledList<ReadSetEntry, 255> read_set_;
	PooledList<WriteSetEntry, 255> write_set_;
	PooledList<WriteData, 255> write_data_;
			
	Xoroshiro128pp rng_;

	inline void CommitData(WriteData& data);
	inline bool ValidateReadSet();

	inline bool Extend();
	inline void Rollback();

	inline void CmOnStart();
	inline void CmOnRestart();
	inline void CmOnWrite();

	inline bool CmShouldAbort(WriteLock& lock);

	inline void MarkAbort();

  public:
	TransactionEngine();
	~TransactionEngine();

	TransactionEngine(const TransactionEngine&) = delete;
	TransactionEngine(TransactionEngine&&) = delete;
	TransactionEngine& operator=(const TransactionEngine&) = delete;
	TransactionEngine& operator=(TransactionEngine&&) = delete;

	void Init();

	inline PromotionState IsReadWriteCompatible() const;
	inline PromotionState IsReadOnlyCompatible() const;

	inline void BeginReadWrite();
	inline void BeginReadOnly();

	inline void Commit();
	inline void End();

	inline Word ReadWord(void* address);
	inline void WriteWord(void* address, Word data, Word mask);

	static inline TransactionEngine& GetThreadEngine();
};

// Keep it within 2 cache lines
static_assert(sizeof(TransactionEngine) <= 128u);


//
// Inline function definitions
//

template<class _Ty, size_t _Cnt>
inline _Ty* PooledList<_Ty, _Cnt>::GetEntry(size_t index) {
	size_t pool = index / _Cnt;
	size_t entry = index % _Cnt;

	if (pool != 0) {
		throw std::runtime_error{ "Out of memory" };
	}

	return reinterpret_cast<_Ty*>(reinterpret_cast<uint8_t*>(main_pool_) + (kEntrySize * entry));
}

template<class _Ty, size_t _Cnt>
PooledList<_Ty, _Cnt>::PooledList() {
}

template<class _Ty, size_t _Cnt>
PooledList<_Ty, _Cnt>::~PooledList() {
	if(main_pool_ != nullptr) {
		for (size_t i{ 0 }; i < _Cnt; i++) {
			GetEntry(i)->~_Ty();
		}

		delete reinterpret_cast<PoolPage*>(main_pool_);
		main_pool_ = nullptr;
	}
}

template<class _Ty, size_t _Cnt>
void PooledList<_Ty, _Cnt>::Init() {
	assert(main_pool_ == nullptr);

	main_pool_ = new PoolPage{};
	for (size_t i{ 0 }; i < _Cnt; i++) {
		new (GetEntry(i))_Ty{};
	}
}

template<class _Ty, size_t _Cnt>
_Ty* PooledList<_Ty, _Cnt>::Create(Key key) {
	size_t index = next_index_++;
	_Ty* entry{ GetEntry(index) };
	*entry = key;
	return entry;
}

template<class _Ty, size_t _Cnt>
_Ty* PooledList<_Ty, _Cnt>::GetOrCreate(Key key) {
	for (size_t i{ 0 }; i < next_index_; i++) {
		_Ty* entry{ GetEntry(i) };
		if (*entry == key) {
			return entry;
		}
	}

	// Create new entry
	size_t index = next_index_++;
	_Ty* entry{ GetEntry(index) };
	*entry = key;
	return entry;
}

template<class _Ty, size_t _Cnt>
_Ty* PooledList<_Ty, _Cnt>::Get(Key key) {
	for (size_t i{ 0 }; i < next_index_; i++) {
		_Ty* entry{ GetEntry(i) };
		if (*entry == key) {
			return entry;
		}
	}
	return nullptr;
}

template<class _Ty, size_t _Cnt>
bool PooledList<_Ty, _Cnt>::Contains(Key key) {
	for (size_t i{ 0 }; i < next_index_; i++) {
		const _Ty* entry{ GetEntry(i) };
		if (*entry == key) {
			return true;
		}
	}
	return false;
}

template<class _Ty, size_t _Cnt>
void PooledList<_Ty, _Cnt>::Clear() {
	next_index_ = 0;
}

template<class _Ty, size_t _Cnt>
bool PooledList<_Ty, _Cnt>::Empty() const {
	return next_index_ == 0;
}

template<class _Ty, size_t _Cnt>
size_t PooledList<_Ty, _Cnt>::GetSize() const {
	return next_index_;
}
		
template<class _Ty, size_t _Cnt>
PooledList<_Ty, _Cnt>::Iterator::Iterator(PooledList& list, size_t start) : list_{ &list }, current_index_{ start } {
}
		
template<class _Ty, size_t _Cnt>
PooledList<_Ty, _Cnt>::Iterator::Iterator(const Iterator& other) : list_{ other.list }, current_index_{ other.current_index } {
}

template<class _Ty, size_t _Cnt>
PooledList<_Ty, _Cnt>::Iterator::Iterator(Iterator&& other) : list_{ other.list }, current_index_{ other.currnet_index } {
}

template<class _Ty, size_t _Cnt>
typename PooledList<_Ty, _Cnt>::Iterator& PooledList<_Ty, _Cnt>::Iterator::operator=(const Iterator& other) {
	list_ = other.list_;
	current_index_ = other.current_index_;
	return *this;
}

template<class _Ty, size_t _Cnt>
typename PooledList<_Ty, _Cnt>::Iterator& PooledList<_Ty, _Cnt>::Iterator::operator=(Iterator&& other) {
	list_ = other.list_;
	current_index_ = other.current_index_;
	return *this;
}

template<class _Ty, size_t _Cnt>
bool PooledList<_Ty, _Cnt>::Iterator::operator==(const Iterator& other) const {
	return current_index_ == other.current_index_;
}

template<class _Ty, size_t _Cnt>
bool PooledList<_Ty, _Cnt>::Iterator::operator!=(const Iterator& other) const {
	return current_index_ != other.current_index_;
}

template<class _Ty, size_t _Cnt>
typename PooledList<_Ty, _Cnt>::Iterator& PooledList<_Ty, _Cnt>::Iterator::operator++() {
	current_index_++;
	return *this;
}

template<class _Ty, size_t _Cnt>
typename PooledList<_Ty, _Cnt>::Iterator::reference PooledList<_Ty, _Cnt>::Iterator::operator*() {
	return *(list_->GetEntry(current_index_));
}

template<class _Ty, size_t _Cnt>
typename PooledList<_Ty, _Cnt>::Iterator PooledList<_Ty, _Cnt>::begin() {
	return Iterator{ *this, 0u };
}

template<class _Ty, size_t _Cnt>
typename PooledList<_Ty, _Cnt>::Iterator PooledList<_Ty, _Cnt>::end() {
	return Iterator{ *this, next_index_ };
}

uint64_t Xoroshiro128pp::Rotl(const uint64_t x, int k) {
	return (x << k) | (x >> (64 - k));
}

Xoroshiro128pp::Xoroshiro128pp() {
	s_[0] = 0xdad6490a0e036cbfu;
	s_[1] = 0x282ef0c42968addcu;
}

Xoroshiro128pp::Xoroshiro128pp(uint64_t s0, uint64_t s1) : s_{ s0, s1 } {
}

uint64_t Xoroshiro128pp::Next() {
	const uint64_t s0 = s_[0];
	uint64_t s1 = s_[1];
	const uint64_t result = Rotl(s0 + s1, 17) + s0;

	s1 ^= s0;
	s_[0] = Rotl(s0, 49) ^ s1 ^ (s1 << 21);
	s_[1] = Rotl(s1, 28);

	return result;
}

void Xoroshiro128pp::Jump() {
    constexpr uint64_t kJump[] = { 0x2bd7a6a6e99c2ddc, 0x0992ccaf6a6fca05 };

	uint64_t s0 = 0;
	uint64_t s1 = 0;
	for(int i = 0; i < sizeof kJump / sizeof *kJump; i++)
		for(int b = 0; b < 64; b++) {
			if (kJump[i] & static_cast<uint64_t>(1u) << b) {
				s0 ^= s_[0];
				s1 ^= s_[1];
			}
			Next();
		}

	s_[0] = s0;
	s_[1] = s1;
}

ReadSetEntry& ReadSetEntry::operator=(Key nindex) {
	index_ = nindex;
	return *this;
}

bool ReadSetEntry::operator==(Key other) const {
	return index_ == other;
}

void ReadSetEntry::SetVersion(Version new_version) noexcept {
	old_version_ = new_version;
}

ReadSetEntry::Key ReadSetEntry::GetIndex() const noexcept {
	return index_;
}

Version ReadSetEntry::GetVersion() const noexcept {
	return old_version_;
}

WriteSetEntry& WriteSetEntry::operator=(Key new_index) {
	index_ = new_index;
	return *this;
}

bool WriteSetEntry::operator==(Key other) const {
	return index_ == other;
}

WriteSetEntry::Key WriteSetEntry::GetIndex() const noexcept {
	return index_;
}


WriteData& WriteData::operator=(Key new_addr) {
	address_ = new_addr;
	return *this;
}

bool WriteData::operator==(Key other) const {
	return address_ == other;
}

void WriteData::Set(Word new_data, Word new_mask) {
	data_ = new_data;
	mask_ = new_mask;
}

void WriteData::Extend(Word new_data, Word new_mask) {
	data_ = (data_ & ~new_mask) | (new_data & new_mask);
	mask_ |= new_mask;
}

WriteData::Key WriteData::GetAddress() const noexcept {
	return address_;
}

Word WriteData::GetData() const noexcept {
	return data_;
}

Word WriteData::GetMask() const noexcept {
	return mask_;
}


void TransactionEngine::CommitData(WriteData& data) {
	volatile Word* addr{ reinterpret_cast<Word*>(data.GetAddress()) };
	*addr = (*addr & ~(data.GetMask())) | (data.GetData() & data.GetMask());
}

bool TransactionEngine::ValidateReadSet() {
	for (ReadSetEntry& entry : read_set_) {
		LockEntry& lock{ lock_table_[entry.GetIndex()] };
		Version v{ lock.r_lock.Get() };
		if (lock.r_lock.Get() != entry.GetVersion()) {
			if (!((v & ReadLock::kLockMask) && lock.w_lock.IsLockedBy(this))) {
				return false;
			}
		}
	}
	return true;
}

bool TransactionEngine::Extend() {
	Version new_version{ GetGlobalVersion() };
	if (ValidateReadSet()) {
		version_ = new_version;
		return true;
	}
	return false;
}

void TransactionEngine::Rollback() {
	for (WriteSetEntry& entry : write_set_) {
		lock_table_[entry.GetIndex()].w_lock.Unlock();
	}

	read_set_.Clear();
	write_set_.Clear();
	write_data_.Clear();
}

void TransactionEngine::CmOnStart() {
	cm_ts_.store(std::numeric_limits<Version>::max());
	cm_backoff_ = 0;
}

void TransactionEngine::CmOnRestart() {
	uint16_t rand = static_cast<uint16_t>(rng_.Next() & 0xF);

	cm_backoff_ += rand;
	std::this_thread::sleep_for(std::chrono::nanoseconds(cm_backoff_));
	cm_backoff_ = cm_backoff_ << 1u;
}

void TransactionEngine::CmOnWrite() {
	if (cm_ts_.load(std::memory_order_relaxed) == std::numeric_limits<Version>::max()) {
		if (write_set_.GetSize() >= 10) {
			cm_ts_.store(GetIncGreedyVersion());
		}
	}
}

bool TransactionEngine::CmShouldAbort(WriteLock& lock) {
	Version ts{ cm_ts_.load(std::memory_order_relaxed) };
	if (ts == std::numeric_limits<Version>::max()) {
		return true;
	}

	TransactionEngine* owner = lock.GetOwner();
	if (owner) {
		if (owner->cm_ts_.load() < ts) {
			return true;
		}

		owner->MarkAbort();
	}

	return false;
}

void TransactionEngine::MarkAbort() {
	// TODO
}

PromotionState TransactionEngine::IsReadWriteCompatible() const {
    if((state_ & State::RUNNING_bit) == State::NONE_mask) {
        return PromotionState::NO_RUNNING;
    }

    if(state_ == State::READ_WRITE_RUNNING) {
        return PromotionState::COMPATIBLE;
    } else {
        return PromotionState::INCOMPATIBLE;
    }
}

PromotionState TransactionEngine::IsReadOnlyCompatible() const {
    if((state_ & State::RUNNING_bit) == State::NONE_mask) {
        return PromotionState::NO_RUNNING;
    }
    
    if((state_ == State::READ_WRITE_RUNNING) || (state_ == State::READ_ONLY_RUNNING)) {
        return PromotionState::COMPATIBLE;
    } else {
        return PromotionState::INCOMPATIBLE;
    }
}

void TransactionEngine::BeginReadWrite() {
	if (state_ == State::READ_WRITE_RUNNING) {
		CmOnRestart();
	}
	else {
		assert(state_ == State::INITIALIZED);
	}

	version_ = GetGlobalVersion();
	state_ = State::READ_WRITE_RUNNING;
}

void TransactionEngine::BeginReadOnly() {
	if (state_ == State::READ_ONLY_RUNNING) {
		CmOnRestart();
	}
	else {
		assert(state_ == State::INITIALIZED);
	}

	version_ = GetGlobalVersion();
	state_ = State::READ_ONLY_RUNNING;
}


void TransactionEngine::Commit() {
	assert((state_ & State::RUNNING_bit) == State::RUNNING_bit);

	if (state_ == State::READ_ONLY_RUNNING) {
		state_ = State::INITIALIZED;
		return;
	}

	if (!write_set_.Empty()) {
		for (WriteSetEntry& entry : write_set_) {
			lock_table_[entry.GetIndex()].r_lock.Lock();
		}

		Version new_version = GetIncGlobalVersion();

		if (new_version > version_ + 1) {
			// Extended validation needed
			if (!ValidateReadSet()) {
				for (WriteSetEntry& entry : write_set_) {
					lock_table_[entry.GetIndex()].r_lock.Unlock();
				}
				Rollback();

				throw TransactionError{ "Failed to validate read set", true };
			}
		}

		for (WriteData& data : write_data_) {
			CommitData(data);
		}

		for (WriteSetEntry& entry : write_set_) {
			LockEntry& lock{ lock_table_[entry.GetIndex()] };
			lock.r_lock.Unlock(new_version);
			lock.w_lock.Unlock();
		}
	}

	read_set_.Clear();
	write_set_.Clear();
	write_data_.Clear();

	state_ = State::INITIALIZED;
}

void TransactionEngine::End() {
	assert((state_ & State::RUNNING_bit) == State::RUNNING_bit);

	for (WriteSetEntry& entry : write_set_) {
		lock_table_[entry.GetIndex()].w_lock.Unlock();
	}

	read_set_.Clear();
	write_set_.Clear();
	write_data_.Clear();

	state_ = State::INITIALIZED;
}

Word TransactionEngine::ReadWord(void* address) {
	LockIndex index{ GetLockIndex(address) };
	LockEntry& lock{ lock_table_[index] };

	if (lock.w_lock.IsLockedBy(this)) {
		WriteData* entry{ write_data_.Get(reinterpret_cast<size_t>(address)) };
		assert(entry != nullptr);
			
		return entry->GetData();
	}

	Word data;

	Version v1 = lock.r_lock.Get();
	while (true) {
		if (v1 & ReadLock::kLockMask) {
			v1 = lock.r_lock.Get();
			continue;
		}

		data = *((volatile Word*) address);

		Version v2 = lock.r_lock.Get();
		if (v2 == v1) {
			break;
		}
		v1 = v2;
	}

	ReadSetEntry* entry{ read_set_.GetOrCreate(index) };
	if (v1 > version_) {
		if (!Extend()) {
			Rollback();
			throw TransactionError{ "Read inconsistent state", true };
		}
	}

	return data;
}

void TransactionEngine::WriteWord(void* address, Word data, Word mask) {
	LockIndex index{ GetLockIndex(address) };
	LockEntry& lock{ lock_table_[index] };

	if (lock.w_lock.IsLockedBy(this)) {
		WriteData* entry{ write_data_.Get(reinterpret_cast<size_t>(address)) };
				
		if (entry == nullptr) {
			entry = write_data_.Create(reinterpret_cast<size_t>(address));
			assert(entry != nullptr);

			entry->Set(data, mask);
		}
		else {
			entry->Extend(data, mask);
		}
		return;
	}

	WriteData* entry;
	while (true) {
		if (lock.w_lock.IsLocked()) {
			if (CmShouldAbort(lock.w_lock)) {
				Rollback();
				throw TransactionError{ "Stuff", true };
			}
			continue;
		}
		if (lock.w_lock.TryLock(this)) {
			write_set_.Create(index);

			entry = write_data_.Create(reinterpret_cast<size_t>(address));
			assert(entry != nullptr);

			break;
		}
	}

	if (lock.r_lock.Get() > version_) {
		if (!Extend()) {
			Rollback();
			throw TransactionError{ "Inconsistent state after write", true };
		}
	}

	if (mask != ~static_cast<Word>(0)) {
		data = (data & mask) | (*((volatile Word*) address) & ~mask);
	}
	entry->Set(data, mask);

	CmOnWrite();
}

TransactionEngine& TransactionEngine::GetThreadEngine() {
	return thread_engine;
}

} // namespace nlane::transactional::detail
