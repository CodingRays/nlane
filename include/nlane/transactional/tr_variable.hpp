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
 * This file contains helper classes for transactional data.
 */

#include <nlane/transactional/transactional.hpp>

namespace nlane {
namespace transactional {

template<class _Ty>
class TRVariable {
    _Ty value_;

  public:
    inline TRVariable();
    inline TRVariable(_Ty value);

    inline TRVariable<_Ty>& operator =(_Ty value);

    inline void Set(_Ty);

    inline operator _Ty() const;

    inline _Ty Get() const;

    inline bool operator ==(_Ty value) const;
    inline bool operator !=(_Ty value) const;

    inline TRVariable<_Ty>& operator ++();
    inline _Ty operator ++(int);
    inline TRVariable<_Ty>& operator --();
    inline _Ty operator --(int);

    inline TRVariable<_Ty>& operator +=(_Ty);
    inline TRVariable<_Ty>& operator -=(_Ty);
    inline TRVariable<_Ty>& operator *=(_Ty);
    inline TRVariable<_Ty>& operator /=(_Ty);

    inline TRVariable<_Ty>& operator |=(_Ty);
    inline TRVariable<_Ty>& operator &=(_Ty);
    inline TRVariable<_Ty>& operator ^=(_Ty);

    /**
     * Reads the variable without using synchronization.
     * 
     * Using this function is EXTREMELY DANGEROUS. It should only
     * be used if no transactional memory context is available and
     * its 100% guaranteed that there is not concurrent access.
     */
    inline _Ty unsafeRead() const;

    /**
     * Writes the variable without using synchronization.
     * 
     * Using this function is EXTREMELY DANGEROUS. It should only
     * be used if no transactional memory context is available and
     * its 100% guaranteed that there is not concurrent access.
     */
    inline void unsafeWrite(_Ty value);
};

} // namespace transactional
} // namespace nlane

//
// Inline function definitions
//

namespace nlane::transactional {

template<class _Ty>
TRVariable<_Ty>::TRVariable() {
}

template<class _Ty>
TRVariable<_Ty>::TRVariable(_Ty value) {
    Set(value)
}

template<class _Ty>
TRVariable<_Ty>::TRVariable(_Ty value, int unsafe) : value_{ value } {
}

template<class _Ty>
void TRVariable<_Ty>::Set(_Ty value) {
    ::nlane::transactional::Write<_Ty>(&value_, value);
}

template<class _Ty>
_Ty TRVariable<_Ty>::Get() const {
    return ::nlane::transactional::Read<_Ty>(&value_);
}

template<class _Ty>
TRVariable<_Ty>& TRVariable<_Ty>::operator =(_Ty value) {
    Set(value);
    return *this;
}

template<class _Ty>
TRVariable<_Ty>::operator _Ty() const {
    return Get();
}

template<class _Ty>
bool TRVariable<_Ty>::operator ==(_Ty value) const {
    return Get() == value;
}

template<class _Ty>
bool TRVariable<_Ty>::operator !=(_Ty value) const {
    return Get() != value;
}

template<class _Ty>
TRVariable<_Ty>& TRVariable<_Ty>::operator ++() {
    Set(Get()++);
    return *this;
}

template<class _Ty>
_Ty TRVariable<_Ty>::operator ++(int) {
    _Ty val{ Get() };
    _Ty ret{ val };
    Set(val++);
    return ret;
}

template<class _Ty>
TRVariable<_Ty>& TRVariable<_Ty>::operator --() {
    Set(Get()--);
    return *this;
}

template<class _Ty>
_Ty TRVariable<_Ty>::operator --(int) {
    _Ty val{ Get() };
    _Ty ret{ val };
    Set(val--);
    return ret;
}

template<class _Ty>
TRVariable<_Ty>& TRVariable<_Ty>::operator +=(_Ty other) {
    Set(Get() + other);
    return *this;
}

template<class _Ty>
TRVariable<_Ty>& TRVariable<_Ty>::operator -=(_Ty other) {
    Set(Get() - other);
    return *this;
}

template<class _Ty>
TRVariable<_Ty>& TRVariable<_Ty>::operator *=(_Ty other) {
    Set(Get() * other);
    return *this;
}

template<class _Ty>
TRVariable<_Ty>& TRVariable<_Ty>::operator /=(_Ty other) {
    Set(Get() / other);
    return *this;
}

template<class _Ty>
TRVariable<_Ty>& TRVariable<_Ty>::operator |=(_Ty other) {
    Set(Get() | other);
    return *this;
}

template<class _Ty>
TRVariable<_Ty>& TRVariable<_Ty>::operator &=(_Ty other) {
    Set(Get() & other);
    return *this;
}

template<class _Ty>
TRVariable<_Ty>& TRVariable<_Ty>::operator ^=(_Ty other) {
    Set(Get() ^ other);
    return *this;
}

} // namespace nlane::transactional