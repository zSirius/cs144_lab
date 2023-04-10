#include "retransmission_timer.hh"

using namespace std;

Retransmission_timer::Retransmission_timer(unsigned int _initial_retransmission_timeout)
    : _rto(_initial_retransmission_timeout) {}

bool Retransmission_timer::get_status() { return _status; }
void Retransmission_timer::set_status(bool status) { _status = status; }

void Retransmission_timer::reset_time() { time = 0; }
void Retransmission_timer::set_time(const size_t ms_since_last_tick) { time += ms_since_last_tick; }

void Retransmission_timer::reset_crt() { continuous_retransmission = 0; }
void Retransmission_timer::increase_crt() { ++continuous_retransmission; }

unsigned int Retransmission_timer::get_rto() { return _rto; }

void Retransmission_timer::set_rto(unsigned int new_rto) { _rto = new_rto; }
bool Retransmission_timer::is_expired() {
    if (time >= _rto)
        return true;
    return false;
}

void Retransmission_timer::reset_timer(unsigned int rto) {
    reset_time();
    set_rto(rto);
}
