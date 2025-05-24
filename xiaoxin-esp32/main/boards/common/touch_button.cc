#include "touch_button.h"
#include <esp_err.h>
#include <driver/touch_pad.h>
static const char* TAG = "TouchButton";

TouchButton::TouchButton(touch_pad_t touch_channel, float threshold_ratio)
    : touch_channel_(touch_channel), threshold_ratio_(threshold_ratio) {
}

TouchButton::~TouchButton() {
    Stop();
    DestroyTimers();
}

bool TouchButton::Init() {
    // 初始化触摸传感器
    esp_err_t ret = touch_pad_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "触摸传感器初始化失败: %s", esp_err_to_name(ret));
        return false;
    }
    
    // 设置触摸传感器的参考电压，使读数更稳定
    ret = touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置触摸传感器电压失败: %s", esp_err_to_name(ret));
        return false;
    }
    
    // 配置触摸通道
    ret = touch_pad_config(touch_channel_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "配置触摸通道失败: %s", esp_err_to_name(ret));
        return false;
    }
    
    // 设置触摸传感器滤波模式
    touch_filter_config_t filter_info = {
        .mode = TOUCH_PAD_FILTER_IIR_16,
        .debounce_cnt = 1,      // 1次测量结果一致则确认状态变化
        .noise_thr = 0,         // 噪声阈值
        .jitter_step = 4,       // 滤波器步长
        .smh_lvl = TOUCH_PAD_SMOOTH_IIR_2,
    };
    ret = touch_pad_filter_set_config(&filter_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置触摸传感器滤波模式失败: %s", esp_err_to_name(ret));
        return false;
    }
    
    ret = touch_pad_filter_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启用触摸传感器滤波失败: %s", esp_err_to_name(ret));
        return false;
    }
    
    // 等待触摸传感器初始化完成
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    return true;
}

void TouchButton::Start() {
    if (running_) {
        return;
    }
    
    // 开始FSM
    esp_err_t ret = touch_pad_fsm_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动触摸传感器FSM失败: %s", esp_err_to_name(ret));
        return;
    }
    
    // 等待触摸基准值稳定
    vTaskDelay(200 / portTICK_PERIOD_MS);
    
    // 读取基准值
    ret = touch_pad_read_benchmark(touch_channel_, &baseline_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读取触摸传感器基准值失败: %s", esp_err_to_name(ret));
        return;
    }
    
   // ESP_LOGI(TAG, "触摸传感器基准值: %"PRIu32, baseline_);
    
    // 修改：阈值计算方式改为基准值的2倍，而不是乘以ratio
    // 当读数大于阈值时视为触摸
    touch_threshold_ = (uint32_t)(baseline_ * 2.0);
    //ESP_LOGI(TAG, "设置触摸阈值: %"PRIu32" (基准值的200%%)", touch_threshold_);
    
    // 创建并启动更新任务
    running_ = true;
    xTaskCreate(UpdateTask, "touch_btn_task", 4096, this, 5, &task_handle_);
}

void TouchButton::Stop() {
    running_ = false;
    
    if (task_handle_ != nullptr) {
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
    }
}

void TouchButton::DestroyTimers() {
    if (timer_handle_ != nullptr) {
        esp_timer_stop(timer_handle_);
        esp_timer_delete(timer_handle_);
        timer_handle_ = nullptr;
    }
}

void TouchButton::UpdateTask(void* arg) {
    TouchButton* button = static_cast<TouchButton*>(arg);
    
    // 首次启动时记录基准信息，不频繁输出日志
    //ESP_LOGI(TAG, "触摸按钮任务启动 - 通道:%d, 基准值:%"PRIu32", 阈值:%"PRIu32, 
    //         button->touch_channel_, button->baseline_, button->touch_threshold_);
    
    // 用于定期输出状态的计数器
    uint32_t status_counter = 0;
    
    while (button->running_) {
        // 执行状态更新
        button->UpdateState();
        

        
        // 增加延时，减少CPU占用
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    
    vTaskDelete(NULL);
}

void TouchButton::ClickTimerCallback(void* arg) {
    TouchButton* button = static_cast<TouchButton*>(arg);
    
    // 如果标记为等待双击，说明已经过了等待时间，可以触发单击回调
    if (button->click_triggered_ && button->on_click_) {
        button->on_click_();
        
    }
    
    // 无论如何都重置标记
    button->click_triggered_ = false;
}

void TouchButton::LongPressTimerCallback(void* arg) {
    TouchButton* button = static_cast<TouchButton*>(arg);
    
    // 只有在触摸状态持续时才触发长按
    if (button->is_touched_ && !button->is_long_pressing_) {
        button->is_long_pressing_ = true;
        
        // 触发长按事件
        if (button->on_long_press_) {
            button->on_long_press_();
            
        }
    }
}

void TouchButton::UpdateState() {
    uint32_t touch_filter_value = 0;
    
    // 读取滤波后的数据
    esp_err_t ret = touch_pad_filter_read_smooth(touch_channel_, &touch_filter_value);
    if (ret != ESP_OK) {
        return;
    }
    
    // 判断是否被触摸（当值低于阈值时视为触摸）
    bool current_touched = (touch_filter_value > touch_threshold_);
    
    // 仅在状态改变时处理
    if (current_touched != prev_touched_) {
        // 简单消抖处理，延时后再次检测
        vTaskDelay(debounce_time_ms_ / portTICK_PERIOD_MS);
        
        // 再次确认当前状态
        ret = touch_pad_filter_read_smooth(touch_channel_, &touch_filter_value);
        if (ret != ESP_OK) {
            return;
        }
        current_touched = (touch_filter_value > touch_threshold_);
        
        // 如果状态确实改变了
        if (current_touched != prev_touched_) {
            // 只在状态变化时输出日志

                     
            prev_touched_ = current_touched;
            is_touched_ = current_touched;
            
            int64_t current_time = esp_timer_get_time() / 1000;  // 转换为毫秒
            
            if (current_touched) {
                // 处理触摸按下事件
                last_press_time_ = current_time;
                is_long_pressing_ = false;
                
                // 复用定时器，避免重复创建
                DestroyTimers();
                
                // 创建长按定时器
                esp_timer_create_args_t timer_args = {
                    .callback = LongPressTimerCallback,
                    .arg = this,
                    .dispatch_method = ESP_TIMER_TASK,
                    .name = "touch_timer",
                    .skip_unhandled_events = true
                };
                esp_timer_create(&timer_args, &timer_handle_);
                esp_timer_start_once(timer_handle_, long_press_time_ms_ * 1000);  // 微秒
                
                // 触发按下事件
                if (on_press_down_) {
                    on_press_down_();
                }
            } else {
                // 处理触摸释放事件
                last_release_time_ = current_time;
                
                // 停止长按定时器
                DestroyTimers();
                
                // 触发释放事件
                if (on_press_up_) {
                    on_press_up_();
                }
                
                // 如果不是长按释放，则处理点击事件
                if (!is_long_pressing_) {
                    if ((current_time - last_release_time_ < double_click_time_ms_) && click_triggered_) {
                        // 处理双击
                        click_triggered_ = false;
                        if (on_double_click_) {
                            on_double_click_();
                        }
                    } else {
                        // 可能是单击，等待双击超时
                        click_triggered_ = true;
                        
                        // 创建单击延时定时器
                        esp_timer_create_args_t timer_args = {
                            .callback = ClickTimerCallback,
                            .arg = this,
                            .dispatch_method = ESP_TIMER_TASK,
                            .name = "touch_timer",
                            .skip_unhandled_events = true
                        };
                        esp_timer_create(&timer_args, &timer_handle_);
                        esp_timer_start_once(timer_handle_, double_click_time_ms_ * 1000);  // 微秒
                    }
                }
                
                is_long_pressing_ = false;
            }
        }
    }
}

void TouchButton::OnPressDown(std::function<void()> callback) {
    on_press_down_ = callback;
}

void TouchButton::OnPressUp(std::function<void()> callback) {
    on_press_up_ = callback;
}

void TouchButton::OnClick(std::function<void()> callback) {
    on_click_ = callback;
}

void TouchButton::OnDoubleClick(std::function<void()> callback) {
    on_double_click_ = callback;
}

void TouchButton::OnLongPress(std::function<void()> callback) {
    on_long_press_ = callback;
} 