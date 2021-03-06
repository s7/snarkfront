#ifndef _SNARKFRONT_DSL_BLESS_HPP_
#define _SNARKFRONT_DSL_BLESS_HPP_

#include <array>
#include <cassert>
#include <cstdint>
#include <vector>
#include "DataBuffer.hpp"
#include "DSL_base.hpp"
#include "DSL_utility.hpp"
#include "PowersOf2.hpp"
#include "R1C.hpp"
#include <sstream>
#include "TLsingleton.hpp"

namespace snarkfront {

////////////////////////////////////////////////////////////////////////////////
// blessing (initialize variables)
//

// variable with value
template <typename FR> void bless(bool_x<FR>& x, const bool a) { x.bless(a); }
template <typename FR> void bless(bigint_x<FR>& x, const std::string& a) { x.bless(a); }
template <typename FR> void bless(uint32_x<FR>& x, const std::uint32_t a) { x.bless(a); }
template <typename FR> void bless(uint64_x<FR>& x, const std::uint64_t a) { x.bless(a); }

template <typename FR> void bless(bigint_x<FR>& x, const std::uint64_t a) {
    std::stringstream ss;
    ss << a;
    x.bless(ss.str());
}

// initialize variable
template <typename FR> void bless(bool_x<FR>& x) { bless(x, false); }
template <typename FR> void bless(bigint_x<FR>& x) { bless(x, "0"); }
template <typename FR> void bless(uint32_x<FR>& x) { bless(x, 0); }
template <typename FR> void bless(uint64_x<FR>& x) { bless(x, 0); }

// array of variables with array of values
template <typename T, typename U, std::size_t N>
void bless(std::array<T, N>& a, const std::array<U, N>& b) {
    for (std::size_t i = 0; i < N; ++i)
        bless(a[i], b[i]);
}

// initialize array of variables
template <typename T, std::size_t N>
void bless(std::array<T, N>& a) {
    for (auto& x : a)
        bless(x);
}

// conversion of:
// - 64-bit word to two 32-bit words
// - 128-bit big integer to four 32-bit words
// - 128 bit big integer to two 64-bit words
template <typename T, std::size_t N, typename U>
void bless(std::array<T, N>& x, const U& a)
{
    const std::size_t
        sizeT = sizeBits(x[0]),
        sizeU = sizeBits(a);

#ifdef USE_ASSERT
    assert(sizeT * N == sizeU);
#endif

    typedef typename T::FrType FR;

    const auto term_bits = TL<R1C<FR>>::singleton()->argBits(*a);
    const auto split_bits = a->splitBits();

    for (std::size_t i = 0; i < N; ++i) {
        const std::vector<typename T::R1T> term_vec(
            term_bits.begin() + sizeT * i,
            term_bits.begin() + sizeT * (i + 1));

        const std::vector<int> split_vec(
            split_bits.begin() + sizeT * i,
            split_bits.begin() + sizeT * (i + 1));

        typename T::ValueType value;
        bitsValue(value, split_vec);

        x[i].bless(value,
                   TL<PowersOf2<FR>>::singleton()->getNumber(split_vec),
                   split_vec,
                   term_vec);
    }
}

// variable from proof inputs
template <typename T, typename FR>
void bless(T& x, const R1Cowitness<FR>& input) {
    x.bless(input);
}

// array of variables from proof input
template <typename T, std::size_t N, typename FR>
void bless(std::array<T, N>& a, const R1Cowitness<FR>& input) {
    for (auto& x : a)
        bless(x, input);
}

// 32-bit word variable from data buffer stream
template <typename FR>
void bless(uint32_x<FR>& x, DataBufferStream& ss) {
    bless(x, ss.getWord<std::uint32_t>());
}

// 64-bit word variable from data buffer stream
template <typename FR>
void bless(uint64_x<FR>& x, DataBufferStream& ss) {
    bless(x, ss.getWord<std::uint64_t>());
}

// 32-bit value from data buffer stream (useful for templates)
void bless(std::uint32_t& a, DataBufferStream& ss);

// 64-bit value from data buffer stream (useful for templates)
void bless(std::uint64_t& a, DataBufferStream& ss);

// array of variables/values from data buffer stream
template <typename T, std::size_t N>
void bless(std::array<T, N>& a, DataBufferStream& ss) {
    for (auto& x : a)
        bless(x, ss);
}

} // namespace snarkfront

#endif
