#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader head = seg.header();
    const string data = seg.payload().copy();
    if (status == "LISTEN" && head.syn) {
        status = "SYN_RECV";
        ISN = head.seqno;
    }
    if (status == "SYN_RECV") {
        uint64_t index = unwrap(head.seqno, ISN, _reassembler.get_exp());
        if (window_size() == 0)
            return;  //窗口为0，丢弃报文，由上层发送ack
        if (!head.syn && index == 0)
            return;  //丢弃非法序列:序号为0但是不是syn位的序列号是非法序列。
        //其他序列可以全部丢给流重组器处理（重复、失序、丢失）
        if (index > 0)
            --index;  //从absolute seq 转换到 stream seq
        if (index >= _reassembler.get_exp() + window_size())
            return;  //窗口外的序列是非法的，保证交给流重组器的序列都是窗口内的
        _reassembler.push_substring(data, index, head.fin);
        if (_reassembler.get_is_eof() &&
            unassembled_bytes() == 0)  //只有收到了fin并且前面的子串全部收到过一次才能进入fin状态
            status = "FIN_RECV";
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (status == "SYN_RECV") {
        return wrap(_reassembler.get_exp() + 1, ISN);
    } else if (status == "FIN_RECV") {
        return wrap(_reassembler.get_exp() + 2, ISN);
    }
    return nullopt;
}

size_t TCPReceiver::window_size() const { return _reassembler.stream_out().remaining_capacity(); }
