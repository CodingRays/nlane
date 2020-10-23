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

#include <nlane/transactional/transaction_engine.hpp>
#include <nlane/transactional/transaction_support.hpp>
#include <nlane/transactional/transactional.hpp>

namespace nlane::transactional {

TransactionError::TransactionError(const std::string& what, const bool retry) : std::runtime_error{ what }, recoverable_{ retry } {
}

TransactionError::TransactionError(const char* what, const bool retry) : std::runtime_error{ what }, recoverable_{ retry } {
}

TransactionError::TransactionError(const TransactionError& other) noexcept : std::runtime_error{ other }, recoverable_{ other.recoverable_ } {
}

TransactionError::~TransactionError() {
}

TransactionError& TransactionError::operator= (const TransactionError& other) noexcept {
	std::runtime_error::operator=(other);
	recoverable_ = other.recoverable_;
	return *this;
}

bool TransactionError::shouldRetry() const noexcept {
	return recoverable_;
}

namespace detail {

PromotionState IsReadWriteCompatible() {
    return TransactionEngine::GetThreadEngine().IsReadWriteCompatible();
}

PromotionState IsReadOnlyCompatible() {
    return TransactionEngine::GetThreadEngine().IsReadOnlyCompatible();
}

void BeginReadWrite() {
    TransactionEngine::GetThreadEngine().BeginReadWrite();
}

void BeginReadOnly() {
    TransactionEngine::GetThreadEngine().BeginReadOnly();
}

void RestartReadWrite() {
    TransactionEngine::GetThreadEngine().BeginReadWrite();
}

void RestartReadOnly() {
    TransactionEngine::GetThreadEngine().BeginReadOnly();
}

void Commit() {
    TransactionEngine::GetThreadEngine().Commit();
}

void End() {
    TransactionEngine::GetThreadEngine().End();
}

} // namespace detail

void ThreadInit() {
    detail::TransactionEngine::GetThreadEngine().Init();
}

Word ReadWord(void* address) {
	return detail::TransactionEngine::GetThreadEngine().ReadWord(address);
}

void WriteWord(void* address, Word data, Word mask) {
	detail::TransactionEngine::GetThreadEngine().WriteWord(address, data, mask);
}

} // namespace nlane::transactional
