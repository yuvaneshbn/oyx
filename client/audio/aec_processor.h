#ifndef CLIENT_AUDIO_AEC_PROCESSOR_H
#define CLIENT_AUDIO_AEC_PROCESSOR_H

#include <cstdint>
#include <memory>
#include <vector>

#include <api/scoped_refptr.h>

namespace webrtc {
class AudioProcessing;
class AudioFrame;
}

class AecProcessor {
public:
    AecProcessor(int sample_rate_hz, int frame_samples);
    ~AecProcessor();

    bool available() const { return ap_ != nullptr; }

    void set_delay_ms(int delay_ms);
    bool process_render(const int16_t* frame, int samples);
    bool process_capture(std::vector<int16_t>& frame);

private:
    int sample_rate_hz_;
    int frame_samples_;
    int stream_delay_ms_ = 80;
    rtc::scoped_refptr<webrtc::AudioProcessing> ap_;
};

#endif
