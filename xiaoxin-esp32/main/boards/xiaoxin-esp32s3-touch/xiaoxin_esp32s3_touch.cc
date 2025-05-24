#include "wifi_board.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "touch_button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include <wifi_station.h>
#include <esp_log.h>
#include <esp_efuse_table.h>
#include <driver/i2c_master.h>

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_gc9a01.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"

#define TAG "XIAOXINESP32S3"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);


class CustomLcdDisplay : public SpiLcdDisplay {
public:
    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle, 
                    esp_lcd_panel_handle_t panel_handle,
                    int width,
                    int height,
                    int offset_x,
                    int offset_y,
                    bool mirror_x,
                    bool mirror_y,
                    bool swap_xy) 
        : SpiLcdDisplay(io_handle, panel_handle, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy,
                    {
                        .text_font = &font_puhui_20_4,
                        .icon_font = &font_awesome_20_4,
                        .emoji_font = font_emoji_64_init(),
                    }) {

        DisplayLockGuard lock(this);
        // 由于屏幕是圆的，所以状态栏需要增加左右内边距
        lv_obj_set_style_pad_left(status_bar_, LV_HOR_RES * 0.33, 0);
        lv_obj_set_style_pad_right(status_bar_, LV_HOR_RES * 0.33, 0);
    }
};

class XIAOXINESP32S3TOUCH : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    std::unique_ptr<TouchButton> touch_button_;
    Display* display_;

    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    // SPI初始化
    void InitializeSpi() {
        ESP_LOGI(TAG, "Initialize SPI bus");
        spi_bus_config_t buscfg = GC9A01_PANEL_BUS_SPI_CONFIG(DISPLAY_SPI_SCLK_PIN, DISPLAY_SPI_MOSI_PIN, 
                                    DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t));
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    // GC9A01初始化
    void InitializeGc9a01Display() {
        ESP_LOGI(TAG, "Init GC9A01 display");

        ESP_LOGI(TAG, "Install panel IO");
        esp_lcd_panel_io_handle_t io_handle = NULL;
        esp_lcd_panel_io_spi_config_t io_config = GC9A01_PANEL_IO_SPI_CONFIG(DISPLAY_SPI_CS_PIN, DISPLAY_SPI_DC_PIN, NULL, NULL);
        io_config.pclk_hz = DISPLAY_SPI_SCLK_HZ;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &io_handle));
    
        ESP_LOGI(TAG, "Install GC9A01 panel driver");
        esp_lcd_panel_handle_t panel_handle = NULL;
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_SPI_RESET_PIN;    // Set to -1 if not use
        panel_config.rgb_endian = LCD_RGB_ENDIAN_BGR;           //LCD_RGB_ENDIAN_RGB;
        panel_config.bits_per_pixel = 16;                       // Implemented by LCD command `3Ah` (16/18)

        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true)); 

        display_ = new SpiLcdDisplay(io_handle, panel_handle,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_20_4,
                                        .icon_font = &font_awesome_20_4,
                                        .emoji_font = font_emoji_64_init(),
                                    });
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
    }

    // 初始化触摸按钮
    void InitializeTouchButton() {
        // ESP32-S3的触摸通道为TOUCH_PAD_NUM0到TOUCH_PAD_NUM14
        // 以下是ESP32-S3上触摸通道与GPIO的对应关系：
        // TOUCH_PAD_NUM0 -> GPIO1
        // TOUCH_PAD_NUM1 -> GPIO2
        // TOUCH_PAD_NUM2 -> GPIO3
        // TOUCH_PAD_NUM3 -> GPIO4
        // TOUCH_PAD_NUM4 -> GPIO5
        // TOUCH_PAD_NUM5 -> GPIO6
        // TOUCH_PAD_NUM6 -> GPIO7
        // TOUCH_PAD_NUM7 -> GPIO8
        // TOUCH_PAD_NUM8 -> GPIO9
        // TOUCH_PAD_NUM9 -> GPIO10
        // TOUCH_PAD_NUM10 -> GPIO11
        // TOUCH_PAD_NUM11 -> GPIO12
        // TOUCH_PAD_NUM12 -> GPIO13
        // TOUCH_PAD_NUM13 -> GPIO14
        
        // 选择要使用的触摸通道 - 使用之前能看到变化的通道7
        const touch_pad_t TOUCH_CHANNEL = TOUCH_PAD_NUM7;  // 使用GPIO8作为触摸输入
        int gpio_num = TOUCH_CHANNEL + 1;  // GPIO编号 = 通道号+1
        
        ESP_LOGI(TAG, "初始化触摸按钮 (通道: %d, 对应GPIO: %d)", TOUCH_CHANNEL, gpio_num);
        
        // 创建触摸按钮 - 阈值比例参数不再使用，但仍保留
        touch_button_ = std::make_unique<TouchButton>(TOUCH_CHANNEL);
        
        // 配置参数
        touch_button_->SetLongPressTime(1500);    // 1.5秒长按
        touch_button_->SetDoubleClickTime(300);   // 300ms双击时间窗口
        
        // 初始化触摸传感器
        if (!touch_button_->Init()) {
            ESP_LOGE(TAG, "初始化触摸按钮失败");
            return;
        }
        
        // 设置事件回调，减少回调中的复杂逻辑
        touch_button_->OnPressDown([]() {
            ESP_LOGI(TAG, "触摸按钮: 按下");
            
            // 可选：播放按下音效（轻微的声音提示）
            // 由于会和点击事件重叠，建议只在调试时启用这个音效
            // Application::GetInstance().PlaySound("button_down.p3");
        });
        
        touch_button_->OnPressUp([]() {
            ESP_LOGI(TAG, "触摸按钮: 释放");
            
            // 可选：播放释放音效
            // 由于会和点击事件重叠，建议只在调试时启用这个音效
            // Application::GetInstance().PlaySound("button_up.p3");
        });
        
        touch_button_->OnClick([]() {
            ESP_LOGI(TAG, "触摸按钮: 单击事件");
            
            // 播放低电量提示音
            auto& app = Application::GetInstance();
            app.PlaySound(Lang::Sounds::P3_WELCOME);
            
            // 切换聊天状态
            
        });
        
        touch_button_->OnDoubleClick([]() {
            ESP_LOGI(TAG, "触摸按钮: 双击事件");
            
            // 播放双击音效
            auto& app = Application::GetInstance();
            app.PlaySound(Lang::Sounds::P3_SUCCESS);
            
            // 双击功能实现
            ESP_LOGI(TAG, "双击功能已触发");
        });
        
        touch_button_->OnLongPress([]() {
            ESP_LOGI(TAG, "触摸按钮: 长按事件");
            // 播放长按音效
            auto& app = Application::GetInstance();
            app.PlaySound(Lang::Sounds::P3_EXCLAMATION);
        });
        
        // 启动触摸监测
        touch_button_->Start();
        ESP_LOGI(TAG, "触摸按钮已启动，请触摸GPIO %d (触摸通道 %d) 进行测试", gpio_num, TOUCH_CHANNEL);
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker")); 
        thing_manager.AddThing(iot::CreateThing("Screen"));   
        //thing_manager.AddThing(iot::CreateThing("Lamp"));  
    }

public:
    XIAOXINESP32S3TOUCH() : boot_button_(BOOT_BUTTON_GPIO) {  
        InitializeCodecI2c();
        InitializeSpi();
        InitializeGc9a01Display();
        InitializeButtons();
        InitializeTouchButton();
        InitializeIot();
        GetBacklight()->RestoreBrightness();
    }

    virtual Led* GetLed() override {
        static SingleLed led_strip(BUILTIN_LED_GPIO);
        return &led_strip;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
    
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

};

DECLARE_BOARD(XIAOXINESP32S3TOUCH);
