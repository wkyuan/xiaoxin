# 编译配置命令

**配置编译目标为 ESP32S3：**

```bash
idf.py set-target esp32s3
```

**打开 menuconfig：**

```bash
idf.py menuconfig
```

**选择板子：**

```
Xiaozhi Assistant -> Board Type -> Movecall Moji 小智AI衍生版
```


**编译：**

```bash
idf.py build
```

# Movecall Moji ESP32-S3开发板

## 硬件简介
Movecall Moji ESP32-S3是一款功能丰富的开发板，具有以下特性：
- ESP32-S3处理器
- 圆形GC9A01 LCD显示屏
- ES8311音频编解码器
- 用于音频输入/输出的I2S接口
- 内置麦克风和扬声器

## 回声消除功能
本开发板支持回声消除（AEC）功能，特别适合实时音频通话场景。回声消除通过以下原理工作：

1. 捕获扬声器输出的音频作为参考信号
2. 同时捕获麦克风输入
3. 将参考信号与麦克风输入一起发送到DSP处理器
4. DSP处理器从麦克风输入中去除参考信号的回声部分

### 实现细节
回声消除通过以下方式实现：
- 创建一个继承自ES8311AudioCodec的MojiAudioCodec类
- 在Read()和Write()方法中添加回声消除逻辑
- 在输入时将麦克风和参考信号合并为双通道数据
- 使用DSP处理器进行回声消除处理

### 使用说明
- 回声消除功能可以通过config.json中的CONFIG_USE_REALTIME_CHAT配置项启用或禁用
- 当启用时，音频输入将包含麦克风和参考信号的双通道数据
- 使用时需要确保DSP库正确链接

## 硬件连接
- 麦克风连接到ES8311的输入端
- 扬声器连接到ES8311的输出端
- I2S接口连接到ESP32-S3的相应GPIO引脚

## 编译注意事项
- 确保在构建时包含moji_audio_codec.h和moji_audio_codec.cc文件
- 如遇到DSP库链接错误，确保以下设置正确：
  1. 在主CMakeLists.txt中添加ESP-DSP组件路径
  2. 在main/CMakeLists.txt中显式添加对esp-dsp的依赖
  3. 创建esp-dsp组件的配置文件