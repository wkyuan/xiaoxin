//
// Created for Movecall Moji ESP32S3
//

#ifndef _MOJI_AUDIO_CODEC_H
#define _MOJI_AUDIO_CODEC_H

#include <mutex>
#include <vector>

#include <driver/gpio.h>
#include <driver/i2s_std.h>

#include "audio_codecs/audio_codec.h"
#include "audio_codecs/es8311_audio_codec.h"

// 扩展ES8311音频编解码器，实现回声消除
class MojiAudioCodec : public Es8311AudioCodec {
private:
    // 回声消除缓冲区和变量
    std::vector<int16_t> output_buffer_;
    std::mutex mutex_;
    int32_t slice_index_ = 0;
    uint64_t time_us_write_ = 0;
    uint64_t time_us_read_ = 0;
    bool use_echo_cancellation_ = false;

    // 原始音频数据
    std::vector<int16_t> mic_buffer_;
    size_t mic_buffer_size_ = 0;

    // 重写读写方法，实现回声消除
    virtual int Read(int16_t* dest, int samples) override;
    virtual int Write(const int16_t* data, int samples) override;

public:
    MojiAudioCodec(void* i2c_master_handle, i2c_port_t i2c_port, 
                  int input_sample_rate, int output_sample_rate,
                  gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, 
                  gpio_num_t dout, gpio_num_t din,
                  gpio_num_t pa_pin, uint8_t es8311_addr, 
                  bool input_reference);

    virtual ~MojiAudioCodec();
};

#endif //_MOJI_AUDIO_CODEC_H 