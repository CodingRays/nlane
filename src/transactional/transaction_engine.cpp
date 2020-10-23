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

#include <mutex>

#include <nlane/transactional/transaction_engine.hpp>

namespace nlane::transactional::detail {

thread_local TransactionEngine thread_engine;


std::once_flag init_flag;

TransactionEngine::TransactionEngine() {
	std::call_once(init_flag, InitSupport);
}

TransactionEngine::~TransactionEngine() {
}

void TransactionEngine::Init() {
	if(state_ != State::UNINITIALIZED) {
		// TODO add error log once logging system is added
		return;
	}

	lock_table_ = GetLockTable();

	read_set_.Init();
	write_set_.Init();
	write_data_.Init();

	// Generate different states for each rng
	static std::atomic<uint32_t> curr_offset{ 0 };

	uint32_t nr{ curr_offset.fetch_add(1u) };
	nr &= (0xFFu); // To prevent long looping during initialization if many threads have been created
	for(uint32_t i{ 0 }; i < nr; i++) {
		rng_.Jump();
	}
	
	state_ = State::INITIALIZED;
}
}