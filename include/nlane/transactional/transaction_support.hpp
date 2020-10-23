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
 * This file contains any global state and types used by the transactional memory system.
 * The transactional memory algorithm used is the SwissTM as described in its original paper 
 * with slight modifications.
 */

#pragma once

#include <atomic>
#include <limits>

#include "transactional.hpp"

namespace nlane::transactional::detail {

class TransactionEngine;

class ReadLock {
  public:
    // The bit wehere the lock is stored. (Different from the lock mask of WriteLock)
    static constexpr Version kLockMask{ std::numeric_limits<Version>::max() ^ (std::numeric_limits<Version>::max() >> 1u) };

  private:
    volatile Version version_{ 0u };

  public:
    // Sets the lock bit. No validity tests are performed
    inline void Lock();

    // Clears the lock bit. No validity tests are performed
    inline void Unlock();

    // Clears the lock bit and updates the version. No validity tests are performed
    inline void Unlock(Version new_version);

    // Returns the current version including lock bit
    inline Version Get() const noexcept;
};

class WriteLock {
  public:
    // The bit where the lock is stored. (Different from the lock mask of ReadLock)
    static constexpr size_t kLockMask{ 0b1u };

  private:
    std::atomic<size_t> value_{ 0u };

  public:
    // Attempts to set the lock bit. Retuns false if the lock bit is already set.
    inline bool TryLock(TransactionEngine* owner);

    // Clears the lock bit. No validity tests are performed
    inline void Unlock();

    // Returns true if the lock bit is set.
    inline bool IsLocked() const;

    // Returns true if the lock bit is set and the owner of the lock is as specified.
    inline bool IsLockedBy(TransactionEngine* owner) const;

    // Returns the current owner of the lock.
    inline TransactionEngine* GetOwner() const;
};

class LockEntry {
public:
    ReadLock r_lock;
    WriteLock w_lock;
};

// Sanity check for memory layout
static_assert(sizeof(LockEntry) == (sizeof(ReadLock) + sizeof(WriteLock)));
static_assert((sizeof(LockEntry) & (sizeof(LockEntry) - 1u)) == 0u); // Ensure size is power of 2

// The size in number of entries of the global lock table
constexpr size_t kLockTableSize{ 4096u };

// Using a bitmask for indexing requires power of 2 size
static_assert((kLockTableSize & (kLockTableSize - 1u)) == 0);

// Bitmask to easily determine the index into the lock table
constexpr size_t kLockTableMask{ kLockTableSize - 1u };

using LockIndex = size_t;

// Returns the index of the lock that locks the specified address
inline LockIndex GetLockIndex(void* address);

// Returns a pointer to the beginning of the lock table.
LockEntry* GetLockTable();

// Returns the current global version
Version GetGlobalVersion();

// Increments the global version and returns its new value.
Version GetIncGlobalVersion();

// Increments the greedy version and returns its new value.
Version GetIncGreedyVersion();

/**
 * Initializes the global support system. I.e. for now just
 * allocates the lock table.
 * 
 * Must only be called once.
 */
void InitSupport();

//
// Inline function definitions
//

void ReadLock::Lock() {
    version_ |= kLockMask;
}

void ReadLock::Unlock() {
    version_ &= ~kLockMask;
}

void ReadLock::Unlock(Version new_version) {
    version_ = new_version;
}

Version ReadLock::Get() const noexcept {
    return version_;
}

bool WriteLock::TryLock(TransactionEngine* owner) {
    size_t expected{ 0u };
    return value_.compare_exchange_strong(expected, reinterpret_cast<size_t>(owner) | kLockMask);
}

void WriteLock::Unlock() {
    value_.store(0u);
}

bool WriteLock::IsLocked() const {
    return (value_.load() & kLockMask) != 0u;
}

bool WriteLock::IsLockedBy(TransactionEngine* owner) const {
    return value_.load() == (reinterpret_cast<size_t>(owner) | kLockMask);
}

TransactionEngine* WriteLock::GetOwner() const {
    return reinterpret_cast<TransactionEngine*>(value_.load() & ~kLockMask);
}

LockIndex GetLockIndex(void* address) {
    return reinterpret_cast<size_t>(address) & kLockTableMask;
}

} // namespace nlane::transactional::detail
