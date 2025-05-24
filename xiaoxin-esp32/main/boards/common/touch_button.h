#ifndef TOUCH_BUTTON_H
#define TOUCH_BUTTON_H

#include <functional>
#include <driver/touch_pad.h>
#include <driver/touch_sensor.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <inttypes.h>
// 触摸按钮状态
enum TouchButtonEvent {
    TOUCH_BUTTON_PRESS_DOWN,   // 触摸开始
    TOUCH_BUTTON_PRESS_UP,     // 触摸结束
    TOUCH_BUTTON_SINGLE_CLICK, // 单击
    TOUCH_BUTTON_DOUBLE_CLICK, // 双击
    TOUCH_BUTTON_LONG_PRESS    // 长按
};

class TouchButton {
public:
    // 构造函数，传入触摸通道和触摸阈值比例(0.0-1.0)
    TouchButton(touch_pad_t touch_channel, float threshold_ratio = 0.7);
    ~TouchButton();

    // 初始化触摸传感器
    bool Init();
    
    // 开始检测
    void Start();
    
    // 停止检测
    void Stop();

    // 事件回调
    void OnPressDown(std::function<void()> callback);
    void OnPressUp(std::function<void()> callback);
    void OnClick(std::function<void()> callback);
    void OnDoubleClick(std::function<void()> callback);
    void OnLongPress(std::function<void()> callback);
    
    // 设置参数
    void SetLongPressTime(uint32_t time_ms) { long_press_time_ms_ = time_ms; }
    void SetDoubleClickTime(uint32_t time_ms) { double_click_time_ms_ = time_ms; }
    void SetThresholdRatio(float ratio) { threshold_ratio_ = ratio; }

private:
    // 触摸传感器通道
    touch_pad_t touch_channel_;
    
    // 触摸阈值比例(原始值*阈值比例=触摸阈值)
    float threshold_ratio_ = 0.7;
    
    // 计算得到的触摸阈值
    uint32_t touch_threshold_ = 0;
    
    // 基准值
    uint32_t baseline_ = 0;
    
    // 当前是否被触摸
    bool is_touched_ = false;
    
    // 上一次的触摸状态
    bool prev_touched_ = false;
    
    // 最后一次触摸按下的时间
    int64_t last_press_time_ = 0;
    
    // 最后一次触摸释放的时间
    int64_t last_release_time_ = 0;
    
    // 是否是长按中
    bool is_long_pressing_ = false;
    
    // 是否已经触发了单击事件(用于避免双击时再次触发单击)
    bool click_triggered_ = false;
    
    // 定时器句柄
    esp_timer_handle_t timer_handle_ = nullptr;
    
    // 任务句柄
    TaskHandle_t task_handle_ = nullptr;
    
    // 是否运行中
    bool running_ = false;
    
    // 各种事件回调
    std::function<void()> on_press_down_ = nullptr;
    std::function<void()> on_press_up_ = nullptr;
    std::function<void()> on_click_ = nullptr;
    std::function<void()> on_double_click_ = nullptr;
    std::function<void()> on_long_press_ = nullptr;
    
    // 参数设置
    uint32_t long_press_time_ms_ = 1000;    // 长按时间
    uint32_t double_click_time_ms_ = 300;   // 双击时间窗口
    uint32_t debounce_time_ms_ = 20;        // 消抖时间
    
    // 状态更新任务
    static void UpdateTask(void* arg);
    
    // 单击延迟定时器回调(用于判断双击)
    static void ClickTimerCallback(void* arg);
    
    // 长按定时器回调
    static void LongPressTimerCallback(void* arg);
    
    // 更新状态
    void UpdateState();
    
    // 释放定时器资源
    void DestroyTimers();
};

#endif // TOUCH_BUTTON_H 