#ifndef UPLOADER_HPP
#define UPLOADER_HPP

#include <cstdint>

class cUploader {
public:
    int  init(void);
    void start(void);
    void stop(void);
    
    // Trigger immediate upload (when buffer is full or some other event added later)
    void trigger(void);
    
    uint32_t get_failure_count(void) const { return failure_count_; }

private:
    static void timer_handler(struct k_timer *timer);
    static void work_handler(struct k_work *work);
    
    int do_upload(void);
    
    bool running_ = false;
    uint32_t failure_count_ = 0;
};

extern cUploader g_uploader;

#endif // UPLOADER_HPP
