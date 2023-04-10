#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : cap(capacity), total_write(0), total_read(0), is_eof(false), stream({}) {
    // DUMMY_CODE(capacity);
}

size_t ByteStream::write(const string &data) {
    size_t write_size = (cap - buffer_size()) >= data.size() ? data.size() : (cap - buffer_size());
    stream += data.substr(0, write_size);
    total_write += write_size;
    return write_size;
    // DUMMY_CODE(data)
    // return {};
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const { return stream.substr(0, len); }

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    total_read += len;
    stream.erase(0, len);
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    size_t read_size = buffer_size() >= len ? len : buffer_size();
    string get = peek_output(read_size);
    pop_output(read_size);
    return get;
}

void ByteStream::end_input() { is_eof = true; }

bool ByteStream::input_ended() const { return is_eof; }

size_t ByteStream::buffer_size() const { return stream.size(); }

bool ByteStream::buffer_empty() const { return stream.empty(); }

bool ByteStream::eof() const { return is_eof && stream.empty(); }

size_t ByteStream::bytes_written() const { return total_write; }

size_t ByteStream::bytes_read() const { return total_read; }

size_t ByteStream::remaining_capacity() const { return cap - stream.size(); }
