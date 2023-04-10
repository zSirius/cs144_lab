#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (!active())
        return;
    _time_since_last_segment_received = 0;
    bool need_send_ack = seg.length_in_sequence_space();  //是否需要再额外发送一个空报文,不对空报文进行确认

    if (seg.header().rst) {
        set_rst(false);
    } else {
        //交给流重组器的报文得是窗口内的报文，窗口外的报文需要丢弃

        //如果是个empty报文，流重组器会忽略掉，因此可以直接交付给receier处理
        _receiver.segment_received(seg);

        //同时打开的情况
        if (TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_SENT && seg.header().syn &&
            !seg.header().ack) {
            _sender.send_empty_segment();
            segment_send_with_ackno_and_win();
            return;
        }

        if (seg.header().ack) {
            //如果ack位有效,则更新ack和win并调用fill_window() （只有第一个请求连接建立的报文ack位无效）
            if (_sender.stream_in().buffer_size() > 0)
                need_send_ack = false;  //如果流中还有字段未发送，会进行捎带确认
            _sender.ack_received(seg.header().ackno, seg.header().win);
        } else {
            //被动打开连接方，SYN_RECV:只调用fill_window()回复连接建立请求
            //_sender.ack_received(seg.header().
            _sender.fill_window();
            need_send_ack = false;
            _is_active = true;
        }
        if (need_send_ack)
            _sender.send_empty_segment();
        segment_send_with_ackno_and_win();

        //进入close_wiat状态，被动关闭无需等待两个2MSL
        if (TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED &&
            TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV) {
            _linger_after_streams_finish = false;
        }

        // last_ack -> closed,被动关闭tcp连接直接关闭无需等待
        if (TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED &&
            TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV && !_linger_after_streams_finish) {
            _is_active = false;
        }

        //其他的关闭状态,即使完成了所有的传输任务也需要等待2MSL
        //和时间流逝有关,因此在tick()方法中实现
    }
}

void TCPConnection::segment_send_with_ackno_and_win() {
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();

        //判断是否是三次握手中的第一次握手
        if (_receiver.ackno().has_value()) {
            //第一个报文既不携带ack也不携带win,因为连接还未建立,让对方以默认窗口大小1回复一个连接建立请求
            seg.header().win = _receiver.window_size();
            seg.header().ackno = _receiver.ackno().value();
            seg.header().ack = true;
        }
        _segments_out.push(seg);
    }
}

bool TCPConnection::active() const { return _is_active; }

size_t TCPConnection::write(const string &data) {
    size_t write_bytes = _sender.stream_in().write(data);
    _sender.fill_window();
    segment_send_with_ackno_and_win();
    return write_bytes;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!active())
        return;
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        while (!_segments_out.empty())
            _segments_out.pop();
        set_rst(true);
        return;
    }

    segment_send_with_ackno_and_win();

    if (TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED &&
        TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV && _linger_after_streams_finish &&
        _time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
        _is_active = false;
        _linger_after_streams_finish = false;  //等待2MSL后，正常结束连接
    }
}

void TCPConnection::set_rst(bool is_send_rst_seg) {
    if (is_send_rst_seg) {
        TCPSegment seg;
        seg.header().rst = true;
        _segments_out.push(seg);
    }
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _is_active = false;
    _linger_after_streams_finish = false;
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    segment_send_with_ackno_and_win();
}

void TCPConnection::connect() {
    ;
    //发送请求建立连接报文
    _sender.fill_window();
    _is_active = true;
    segment_send_with_ackno_and_win();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            set_rst(false);
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
