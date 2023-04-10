#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), cur_cap(capacity), exp_idx(0), sub(), is_eof(0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // out_put可能从满的变成有空间，检查是否能输出有序子串
    deliver();
    string data_t(data);
    size_t index_t = index;
    if (cur_cap == 0 && index == exp_idx) {
        //空间满了并且没有可以交付的有序子串，但是新来的子串可以满足交付
        next(data_t, index_t);
        //处理完把可以交付的部分直接交付，这样就不用裁剪别的子串, 交付不了的就丢掉
        string de_str = data_t.substr(0, _output.remaining_capacity());
        _output.write(de_str);
        exp_idx += de_str.size();
    }
    deliver();  //交付完新串可能会导致map中的第一个序列可以继续交付
    if (cur_cap == 0 || (index < exp_idx && index + data.size() <= exp_idx))
        return;  //丢弃子串

    if (index <= exp_idx) {
        data_t.erase(0, exp_idx - index);  //把exp_idx前面的子串裁剪掉
        index_t = exp_idx;
    }
    next(data_t, index_t);
    front(data_t, index_t);
    //判断裁剪完的子串长度是否满足cur_cap的要求
    //由于裁剪完的子串已经和map里的子串没有重复元素，因此如果由于长度问题裁剪子串会导致数据丢失

    if (data_t.size() >= cur_cap)
        data_t.erase(data_t.end() - (data_t.size() - cur_cap), data_t.end());
    if (!data_t.empty()) {
        //子串原本非空但是被裁剪空了，说明当前子串在map中已经有了
        sub[index_t] = data_t;  //如果裁剪完不是空串，就插入新的子串
        cur_cap -= data_t.size();
    }
    if (eof)
        is_eof = true;  //存在data是只带一个eof标记的空串
    //有新子串进入就可能有新的按序字节流产生
    deliver();
    if (is_eof && unassembled_bytes() == 0)
        _output.end_input();
}
void StreamReassembler::deliver() {
    while (sub.size() && (exp_idx == sub.begin()->first)) {
        if (_output.remaining_capacity() == 0)
            return;
        //要判断当前子串能否完整地写入,若不能完整地写入需要裁剪
        if (_output.remaining_capacity() >= sub.begin()->second.size()) {
            _output.write(sub.begin()->second);
            exp_idx += sub.begin()->second.size();
            cur_cap += sub.begin()->second.size();
            sub.erase(sub.begin());

        } else {
            string str = sub.begin()->second.substr(0, _output.remaining_capacity());
            string data_new = sub.begin()->second.erase(0, _output.remaining_capacity());
            sub.erase(sub.begin());
            exp_idx += _output.remaining_capacity();
            sub[exp_idx] = data_new;  //键值也要修改，但是map的键值是const，所以需要弹出旧的插入新的
            cur_cap += _output.remaining_capacity();
            _output.write(str);
        }
    }
}

void StreamReassembler::next(string &data, size_t &index) {
    //处理后元素
    auto iter = sub.lower_bound(index);
    if (iter == sub.end())
        return;
    auto first = iter->first;
    auto second = iter->second;
    if (index + data.size() <= first)
        return;  //不需要裁剪
    else if (index + data.size() < first + second.size()) {
        data.erase(data.begin() + first - index, data.end());
        return;
    } else {
        //丢弃后元素前要处理cur_cap
        cur_cap += second.size();
        sub.erase(first);   //丢弃后元素
        next(data, index);  //递归处理
    }
}

void StreamReassembler::front(string &data, size_t &index) {
    auto iter = sub.lower_bound(index);
    if (iter == sub.begin())
        return;  //不存在前元素
    --iter;
    auto first = iter->first;
    auto second = iter->second;
    if (first + second.size() <= index)
        return;
    else if (first + second.size() < index + data.size()) {
        data.erase(data.begin(), data.begin() + first + second.size() - index);
        index = first + second.size();
    } else {
        //被前元素包围，丢弃
        data.clear();
        return;
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _capacity - cur_cap; }

bool StreamReassembler::empty() const { return cur_cap == _capacity; }
