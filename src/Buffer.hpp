#ifndef BUFFER_HPP
#define BUFFER_HPP

#include "types.hpp"
#include <cstddef>

// Ring buffer - stores sensor samples in RAM, lost on reset. 
// Better solustion would be storing it in flash.

class cBuffer {
public:
    int    init(void);
    int    push(const SensorSample *sample);
    size_t peek(SensorSample *samples, size_t max_count);
    void   pop(size_t count);
    void   clear(void);
    
    size_t count(void) const { return count_; }
    size_t capacity(void) const;
    bool   is_empty(void) const { return count_ == 0; }
    int    fill_percent(void) const;

private:
    size_t count_ = 0;
};

extern cBuffer g_buffer;

#endif // BUFFER_HPP
