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

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include <nlane/transactional/transactional.hpp>
#include <nlane/util/random.hpp>

namespace nlane_test::transactional {

using namespace nlane;

class TransactionalTest : public ::testing::Test {
  protected:
    TransactionalTest() {
    }

    ~TransactionalTest() override {
    }

    void SetUp() override {
        tr::ThreadInit();
    }

    void TearDown() override {
    }
};

TEST_F(TransactionalTest, WordReadOnly) {
    constexpr size_t kEntries{ 16u };
    tr::Word words[kEntries];
    
    for(size_t i{ 0 }; i < kEntries; i++) {
        words[i] = static_cast<tr::Word>(i);
    }

    tr::AtomicRead([&]() {
        for(size_t i{ 0 }; i < kEntries; i++) {
            ASSERT_EQ(tr::ReadWord(words + i), i);
        }
    });
}

TEST_F(TransactionalTest, WordReadWrite) {
    constexpr size_t kEntries{ 16u };
    tr::Word words[kEntries];

    for(size_t i{ 0 }; i < kEntries; i++) {
        words[i] = static_cast<tr::Word>(i);
    }

    tr::Atomic([&]() {
        for(size_t i{ 0 }; i < kEntries; i++) {
            ASSERT_EQ(tr::ReadWord(words + i), i);
        }

        for(size_t i{ 0 }; i < kEntries; i+=2) {
            tr::WriteWord(words + i, i * 2u, ~static_cast<tr::Word>(0u));
        }

        for(size_t i{ 0 }; i < kEntries; i++) {
            if((i % 2) == 0) {
                ASSERT_EQ(tr::ReadWord(words + i), i * 2u);
            } else {
                ASSERT_EQ(tr::ReadWord(words + i), i);
            }
        }
    });

    for(size_t i{ 0 }; i < kEntries; i++) {
        if((i % 2) == 0) {
            ASSERT_EQ(words[i], i * 2u);
        } else {
            ASSERT_EQ(words[i], i);
        }
    }

    tr::Atomic([&]() {
        for(size_t i{ 1 }; i < kEntries; i+=2) {
            tr::WriteWord(words + i, i * 2u, ~static_cast<tr::Word>(0u));
        }
        
        for(size_t i{ 0 }; i < kEntries; i++) {
            ASSERT_EQ(tr::ReadWord(words + i), i * 2u);
        }
    });
    
    for(size_t i{ 0 }; i < kEntries; i++) {
        ASSERT_EQ(words[i], i * 2u);
    }
}

template<class _Ty, size_t kEntries>
inline void SimpleNumberRead() {
    _Ty words[kEntries];
    
    for(size_t i{ 0 }; i < kEntries; i++) {
        words[i] = static_cast<_Ty>(i);
    }

    tr::AtomicRead([&]() {
        for(size_t i{ 0 }; i < kEntries; i++) {
            ASSERT_EQ(tr::Read<_Ty>(words + i), static_cast<_Ty>(i));
        }
    });
}

template<class _Ty, size_t kEntries>
inline void SimpleNumberReadWrite() {
    _Ty words[kEntries];

    for(size_t i{ 0 }; i < kEntries; i++) {
        words[i] = static_cast<_Ty>(i);
    }

    tr::Atomic([&]() {
        for(size_t i{ 0 }; i < kEntries; i++) {
            ASSERT_EQ(tr::Read<_Ty>(words + i), static_cast<_Ty>(i));
        }

        for(size_t i{ 0 }; i < kEntries; i+=2) {
            tr::Write<_Ty>(words + i, static_cast<_Ty>(i * 2u));
        }

        for(size_t i{ 0 }; i < kEntries; i++) {
            if((i % 2) == 0) {
                ASSERT_EQ(tr::Read<_Ty>(words + i), static_cast<_Ty>(i * 2u));
            } else {
                ASSERT_EQ(tr::Read<_Ty>(words + i), static_cast<_Ty>(i));
            }
        }
    });

    for(size_t i{ 0 }; i < kEntries; i++) {
        if((i % 2) == 0) {
            ASSERT_EQ(words[i], static_cast<_Ty>(i * 2u));
        } else {
            ASSERT_EQ(words[i], static_cast<_Ty>(i));
        }
    }

    tr::Atomic([&]() {
        for(size_t i{ 1 }; i < kEntries; i+=2) {
            tr::Write<_Ty>(words + i, static_cast<_Ty>(i * 2u));
        }
        
        for(size_t i{ 0 }; i < kEntries; i++) {
            ASSERT_EQ(tr::Read<_Ty>(words + i), static_cast<_Ty>(i * 2u));
        }
    });
    
    for(size_t i{ 0 }; i < kEntries; i++) {
        ASSERT_EQ(words[i], static_cast<_Ty>(i * 2u));
    }
}

TEST_F(TransactionalTest, UI64ReadOnly) {
    SimpleNumberRead<uint64_t, 16>();
}

TEST_F(TransactionalTest, UI64ReadWrite) {
    SimpleNumberReadWrite<uint64_t, 16>();
}

TEST_F(TransactionalTest, I64ReadOnly) {
    SimpleNumberRead<int64_t, 16>();
}

TEST_F(TransactionalTest, I64ReadWrite) {
    SimpleNumberReadWrite<int64_t, 16>();
}

TEST_F(TransactionalTest, UI32ReadOnly) {
    SimpleNumberRead<uint32_t, 32>();
}

TEST_F(TransactionalTest, UI32ReadWrite) {
    SimpleNumberReadWrite<uint32_t, 32>();
}

TEST_F(TransactionalTest, I32ReadOnly) {
    SimpleNumberRead<int32_t, 32>();
}

TEST_F(TransactionalTest, I32ReadWrite) {
    SimpleNumberReadWrite<int32_t, 32>();
}

TEST_F(TransactionalTest, UI16ReadOnly) {
    SimpleNumberRead<uint16_t, 64>();
}

TEST_F(TransactionalTest, UI16ReadWrite) {
    SimpleNumberReadWrite<uint16_t, 64>();
}

TEST_F(TransactionalTest, I16ReadOnly) {
    SimpleNumberRead<int16_t, 64>();
}

TEST_F(TransactionalTest, I16ReadWrite) {
    SimpleNumberReadWrite<int16_t, 64>();
}

TEST_F(TransactionalTest, UI8ReadOnly) {
    SimpleNumberRead<uint16_t, 128>();
}

TEST_F(TransactionalTest, UI8ReadWrite) {
    SimpleNumberReadWrite<uint16_t, 128>();
}

TEST_F(TransactionalTest, I8ReadOnly) {
    SimpleNumberRead<int16_t, 128>();
}

TEST_F(TransactionalTest, I8ReadWrite) {
    SimpleNumberReadWrite<int16_t, 128>();
}

TEST_F(TransactionalTest, HammerCorrectness) {
    constexpr size_t kNumEntries{ 4u };
    constexpr size_t kNumThreads{ 8u };

    uint64_t entries[kNumEntries];
    std::thread threads[kNumThreads];

    for(uint64_t& v : entries) {
        v = 64u;
    }

    std::atomic<bool> run{ false };
    for(std::thread& t : threads) {
        t = std::thread{[&]() {
            tr::ThreadInit();

            while(run.load() == false) {
            }

            uint64_t* e{ entries };
            while(run.load()) {
                size_t e1{ util::Rand() % kNumEntries };
                size_t e2{ util::Rand() % kNumEntries };

                if(e1 == e2) {
                    e2 = (e1 + 1u) % kNumEntries;
                }

                uint64_t amount{ util::Rand() % 32u };

                tr::Atomic([&]() {
                    uint64_t v1{ tr::Read(e + e1) };

                    if(v1 >= amount) {
                        uint64_t v2{ tr::Read(e + e2) };

                        v1 -= amount;
                        v2 += amount;

                        tr::Write(e + e1, v1);
                        tr::Write(e + e2, v2);
                    }
                });
            }
        }};
    }

    auto end{ std::chrono::steady_clock::now() + std::chrono::seconds(2) };

    run.store(true);
    std::this_thread::sleep_until(end);

    run.store(false);
    for(std::thread& t : threads) {
        t.join();
    }

    uint64_t sum{ 0u };
    for(uint64_t v : entries) {
        sum += v;
    }

    ASSERT_EQ(sum, 64u * kNumEntries);
}

} // namespace nlane_test::transactional