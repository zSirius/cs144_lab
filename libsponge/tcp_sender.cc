#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _re_timer(retx_timeout)
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _outstanding_bytes; }

void TCPSender::fill_window() {
    TCPSegment segs;
    uint16_t max_payload = 1452;

    while (!_has_syn || (_stream.buffer_size() != 0 && _sent_window != 0) ||
           (_stream.eof() && _sent_window > 0 && !_has_fin)) {
        if (!_has_syn) {  //如果还未发送syn
            segs.header().syn = true;
            _has_syn = true;
            --_sent_window;  // syn需要占用一个窗口和序列
        } else {
            segs.header().syn = false;
            // segs.header().ack = true;
        }

        uint16_t payload_len = min(_sent_window, max_payload);

        string payload = _stream.read(payload_len);
        segs.payload() = Buffer(move(payload));  //注意移动构造函数的特性，这部执行过后payload就变成了空串
        segs.header().seqno = wrap(_next_seqno, _isn);

        _sent_window -= segs.payload().size();

        if (_stream.eof() && _sent_window > 0) {
            segs.header().fin = true;
            --_sent_window;
            _has_fin = true;
        } else
            segs.header().fin = false;  //如果没读完或者窗口不够，就先不发送fin

        _next_seqno += segs.length_in_sequence_space();

        _segments_out.push(segs);
        _outstanding.push(segs);
        _outstanding_bytes += segs.length_in_sequence_space();

        //重传计时器如果还没运行，就启动
        if (!_re_timer.get_status()) {
            _re_timer.set_status(true);
            _re_timer.reset_timer(_initial_retransmission_timeout);
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    //根据ackno判断outstanding队列是否有报文可以确认
    TCPSegment segs;
    uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);
    //非法/过期的ack需要忽略
    if (abs_ackno > _next_seqno || abs_ackno < ack_now)
        return;
    ack_now = abs_ackno;
    while (!_outstanding.empty()) {
        segs = _outstanding.front();
        if (segs.header().seqno.raw_value() + segs.length_in_sequence_space() > ackno.raw_value())
            break;
        _outstanding_bytes -= segs.length_in_sequence_space();
        _outstanding.pop();
        //重置计时器
        _re_timer.reset_timer(_initial_retransmission_timeout);
        _re_timer.reset_crt();
    }

    if (_outstanding.empty()) {  //没有未完成的段就停止计时器了
        _re_timer.set_status(false);
    }
    //根据窗口发送报文
    //_sent_window维护的是还能发送多少个字节
    if (window_size > 0) {
        _sent_window = ackno.raw_value() + window_size - wrap(_next_seqno, _isn).raw_value();
        is_back_off = true;
    } else {
        if (_outstanding.size() == 0)
            _sent_window = 1;
        else
            _sent_window = 0;
        is_back_off = false;
    }
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_re_timer.get_status())
        return;
    _re_timer.set_time(ms_since_last_tick);
    if (_re_timer.is_expired()) {
        _segments_out.push(_outstanding.front());
        _re_timer.increase_crt();
        if (is_back_off)
            _re_timer.reset_timer(2 * _re_timer.get_rto());
        else
            _re_timer.reset_timer(_re_timer.get_rto());  //不退避RTO(back off)
    }
}
//连续重传数量
unsigned int TCPSender::consecutive_retransmissions() const { return _re_timer.get_crt(); }

void TCPSender::send_empty_segment() {
    //不需要重传
    TCPSegment segs;
    segs.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(segs);
}
