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

#include <nlane/util/random.hpp>

namespace nlane::util {

class alignas(64) PRNG {
  private:
    uint64_t s_[8]{ 0xed114a1b1329f214, 0x1b427ba78e4b653d, 0xfce4fff14ee4f6b8, 0x12e92ae6e6d06e93, 0x0024f3617b58cad1, 0xc4322d77c43148b3, 0x212a99a34d466ac7, 0x785347b3b1c0e816 };
    
    static inline uint64_t Rotl(const uint64_t x, int k) {
	    return (x << k) | (x >> (64 - k));
    }

  public:
    PRNG() {
    }

    PRNG(int use_global_reference);

    inline uint64_t Next() {
        const uint64_t result = Rotl(s_[0] + s_[2], 17) + s_[2];

	    const uint64_t t = s_[1] << 11;

	    s_[2] ^= s_[0];
	    s_[5] ^= s_[1];
	    s_[1] ^= s_[2];
	    s_[7] ^= s_[3];
	    s_[3] ^= s_[4];
	    s_[4] ^= s_[5];
	    s_[0] ^= s_[6];
	    s_[6] ^= s_[7];

	    s_[6] ^= t;

	    s_[7] = Rotl(s_[7], 21);

	    return result;
    }

    inline void Jump() {
        static constexpr uint64_t JUMP[] = { 0x33ed89b6e7a353f9, 0x760083d7955323be, 0x2837f2fbb5f22fae, 0x4b8c5674d309511c, 0xb11ac47a7ba28c25, 0xf1be7667092bcc1c, 0x53851efdb6df0aaf, 0x1ebbc8b23eaf25db };

	    uint64_t t[sizeof(s_) / sizeof(*s_)];
	    memset(t, 0, sizeof(t));
	    for(int i = 0; i < sizeof(JUMP) / sizeof(*JUMP); i++) {
		    for(int b = 0; b < 64; b++) {
			    if (JUMP[i] & UINT64_C(1) << b) {
				    for(int w = 0; w < sizeof(s_) / sizeof(*s_); w++) {
					    t[w] ^= s_[w];
                    }
                }
			    Next();
		    }
        }
	
	    memcpy(s_, t, sizeof(s_));	
    }
};

PRNG global_rng;
std::mutex global_rng_mutex;

PRNG::PRNG(int) {
    std::lock_guard lock{ global_rng_mutex };
    *this = global_rng;
    global_rng.Jump();
}

thread_local PRNG prng{0};

uint64_t Rand() {
    return prng.Next();
}
}