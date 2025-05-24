//
// Created for Movecall Moji ESP32S3
//

#include "moji_audio_codec.h"
#include <esp_log.h>
#include <cmath>
#include <cstring>

#define TAG "MojiAudioCodec"

MojiAudioCodec::MojiAudioCodec(void* i2c_master_handle, i2c_port_t i2c_port, 
                             int input_sample_rate, int output_sample_rate,
                             gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, 
                             gpio_num_t dout, gpio_num_t din,
                             gpio_num_t pa_pin, uint8_t es8311_addr, 
                             bool input_reference)
    : Es8311AudioCodec(i2c_master_handle, i2c_port, input_sample_rate, output_sample_rate,
                      mclk, bclk, ws, dout, din, pa_pin, es8311_addr) {
    // 初始化回声消除相关变量
    use_echo_cancellation_ = input_reference;
    if (use_echo_cancellation_) {
        input_reference_ = true;
        input_channels_ = 2; // 设置为2个通道：麦克风 + 参考信号
        
        // 初始化缓冲区
        const int32_t play_size = 512;
        output_buffer_.resize(play_size * 10, 0);
        mic_buffer_.resize(play_size * 2, 0);
        mic_buffer_size_ = 0;
        slice_index_ = 0;
        
        ESP_LOGI(TAG, "Echo cancellation enabled with reference channel");
    } else {
        ESP_LOGI(TAG, "Echo cancellation disabled");
    }
}

int MojiAudioCodec::Write(const int16_t* data, int samples) {
    // 如果启用了回声消除，将数据保存到参考缓冲区
    if (use_echo_cancellation_) {
        const int32_t play_size = 512;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            for (int i = 0; i < samples; i++) {
                output_buffer_[slice_index_] = data[i];
                slice_index_++;
                if (slice_index_ >= play_size * 10) {
                    slice_index_ = 0;
                }
            }
            time_us_write_ = esp_timer_get_time(); // 获取微秒级时间戳
        }
    }
    
    // 调用父类方法发送音频数据
    return Es8311AudioCodec::Write(data, samples);
}

int MojiAudioCodec::Read(int16_t* dest, int samples) {
    if (!use_echo_cancellation_) {
        // 不使用回声消除，直接使用父类方法
        return Es8311AudioCodec::Read(dest, samples);
    }
    
    // 使用回声消除功能
    static int32_t ref_index = 0;
    static bool first_speak = true;
    const int32_t play_size = 512;
    
    // 更新参考信号时间戳
    {
        std::unique_lock<std::mutex> lock(mutex_);
        time_us_read_ = esp_timer_get_time();
        
        // 如果超过100ms没有语音，清空缓冲区
        if (time_us_read_ - time_us_write_ > 1000 * 100) {
            std::fill(output_buffer_.begin(), output_buffer_.end(), 0);
            first_speak = true;
            slice_index_ = 0;
            ref_index = play_size * 10 - 512;
        } else if (first_speak) {
            first_speak = false;
            ref_index = 0;
        }
        
        if (ref_index < 0) {
            ref_index = play_size * 10 + ref_index;
        }
    }
    
    // 读取原始麦克风数据
    int read_samples = 0;
    
    // 分配临时缓冲区来存储原始麦克风数据
    std::vector<int16_t> tmp_buffer(samples);
    read_samples = Es8311AudioCodec::Read(tmp_buffer.data(), samples / 2);
    
    // 合并麦克风数据和参考数据
    for (int i = 0; i < read_samples; i++) {
        // 麦克风数据
        dest[i * 2] = tmp_buffer[i];
        
        // 参考数据（扬声器输出）
        if (output_buffer_.size() > ref_index) {
            dest[i * 2 + 1] = output_buffer_[ref_index];
        } else {
            dest[i * 2 + 1] = 0;
        }
        
        ref_index++;
        if (ref_index >= play_size * 10) {
            ref_index = ref_index - play_size * 10;
        }
    }
    
    // 返回合并后的数据大小（麦克风 + 参考）
    return read_samples * 2;
}

MojiAudioCodec::~MojiAudioCodec() {
    // 清空缓冲区
    output_buffer_.clear();
    mic_buffer_.clear();
} 