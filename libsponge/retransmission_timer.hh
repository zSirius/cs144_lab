#include <cstddef>
#ifndef SPONGE_LIBSPONGE_RETRANSMISSION_TIMER_HH
#define SPONGE_LIBSPONGE_RETRANSMISSION_TIMER_HH

class Retransmission_timer {
  private:
    size_t time{0};
    unsigned int _rto;
    unsigned int continuous_retransmission{0};  // crt
    bool _status{false};

  public:
    Retransmission_timer(unsigned int _initial_retransmission_timeout);

    unsigned int get_crt() { return continuous_retransmission; }
    unsigned int get_crt() const { return continuous_retransmission; }

    bool get_status();
    void set_status(bool status);
    void reset_time();
    void set_time(const size_t ms_since_last_tick);
    void reset_crt();
    void increase_crt();

    void set_rto(unsigned int new_rto);
    unsigned int get_rto();
    bool is_expired();

    void reset_timer(unsigned int rto);
};

#endif
