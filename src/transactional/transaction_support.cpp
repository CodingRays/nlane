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

#include <atomic>

#include <nlane/transactional/transaction_support.hpp>

namespace nlane::transactional::detail {
std::atomic<Version> global_version{ 0 };

Version GetGlobalVersion() {
	return global_version.load();
}

Version GetIncGlobalVersion() {
	return global_version.fetch_add(1u) + 1u;
}

std::atomic<Version> greedy_version{ 0 };

Version GetIncGreedyVersion() {
	return greedy_version.fetch_add(1u);
}

LockEntry* global_lock_table{ nullptr };

LockEntry* GetLockTable() {
	return global_lock_table;
}

void InitSupport() {
	// TODO better allocation?
	if(global_lock_table != nullptr) {
		throw std::runtime_error{"This shouldnt happen"};
	}
	global_lock_table = new LockEntry[kLockTableSize];
}
}