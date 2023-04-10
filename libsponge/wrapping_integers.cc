#include "wrapping_integers.hh"

#include <cmath>
// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint32_t n_32 = n;
    WrappingInt32 res = isn + n_32;  //溢出自动截断当作取模
    return res;
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint32_t offset = n - isn;
    uint64_t t = offset + (checkpoint & 0xFFFFFFFF00000000), cal = 1ul << 32;
    uint64_t res = t;
    if ((checkpoint >= t) && labs(checkpoint - t) > labs(t + cal - checkpoint))
        res = t + cal;
    if (t >= cal && (checkpoint < t) && labs(t - checkpoint) > labs(checkpoint - t + cal))
        res = t - cal;
    return res;
}
