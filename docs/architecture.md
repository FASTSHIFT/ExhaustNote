# ExhaustNote 系统架构设计

> 版本: 0.1 Draft  
> 日期: 2026-05-30  
> 状态: 设计阶段

---

## 1. 项目概述

ExhaustNote 是一个开源的 MCU 引擎声音模拟器，目标是在低成本嵌入式硬件上实现**多层交叉淡入**（Multi-layer Crossfade）引擎声音合成，达到接近赛车模拟游戏的音质水平。

### 1.1 设计目标

| 指标 | 目标值 |
|------|--------|
| 音频精度 | 16-bit 44.1kHz 立体声 |
| 引擎采样层数 | ≥6 层 RPM + 2 层 Load (on/off throttle) |
| 延迟 | <10ms（输入→声音响应） |
| 硬件成本 | ≤ ¥30 BOM |
| 车辆切换 | <2s（SD 卡→PSRAM 预加载） |

### 1.2 核心算法

```
多层交叉淡入:
  对于 N 个 RPM 采样层 (rpm_0, rpm_1, ..., rpm_N-1):
    找到当前 RPM 所在的相邻两层 layer_lo, layer_hi
    mix = (current_rpm - layer_lo.rpm) / (layer_hi.rpm - layer_lo.rpm)
    output = layer_lo.sample × (1 - mix) + layer_hi.sample × mix

  Load 混合:
    output = onload × throttle + offload × (1 - throttle)

  基频追踪 EQ:
    center_freq = RPM × cylinders / 120  (Hz)
```

---

## 2. 系统架构

### 2.1 分层架构图

```
┌─────────────────────────────────────────────────────┐
│                    app/ (应用层)                      │
│          sim 模拟器入口  |  MCU 固件入口              │
├─────────────────────────────────────────────────────┤
│                   core/ (核心算法)                    │
│  EngineVoice | Crossfade | SamplePlayer | DSP | Mixer│
├─────────────────────────────────────────────────────┤
│                platform/ (平台抽象)                   │
│         AudioOutput | Storage | Input | Timer        │
├──────────────────────┬──────────────────────────────┤
│    platform/sim/     │       platform/mcu/           │
│  SDL2 / PortAudio    │    I2S DMA + PSRAM + SBUS    │
└──────────────────────┴──────────────────────────────┘
```

### 2.2 设计原则

1. **core/ 零平台依赖** — 仅使用 `<cstdint>`, `<cstring>`, `<cmath>`，不包含任何 HAL/OS 头文件
2. **接口隔离** — platform 层定义纯虚接口，sim 和 MCU 各自实现
3. **同源双目标** — 同一份 core 代码编译到 sim (桌面端模拟器) 和 MCU (Cortex-M4F)
4. **测试即文档** — 单元测试覆盖所有核心算法，可在桌面端秒级运行

---

## 3. 目录结构

```
ExhaustNote/
├── CMakeLists.txt                    ← 顶层 CMake
├── README.md
├── LICENSE                           ← MIT License
│
├── core/                             ← 平台无关核心算法库
│   ├── CMakeLists.txt
│   ├── include/core/
│   │   ├── engine_voice.h            ← 多层交叉淡入引擎合成
│   │   ├── crossfade.h               ← 交叉淡入数学运算
│   │   ├── sample_player.h           ← 采样回放 (定速率循环)
│   │   ├── transmission.h            ← 变速箱模拟
│   │   ├── dsp.h                     ← biquad EQ / 滤波器
│   │   ├── mixer.h                   ← 多通道混音 + 限幅
│   │   ├── vehicle.h                 ← 车辆参数配置
│   │   └── types.h                   ← 公共类型定义
│   └── src/
│       ├── engine_voice.cpp
│       ├── crossfade.cpp
│       ├── sample_player.cpp
│       ├── transmission.cpp
│       ├── dsp.cpp
│       └── mixer.cpp
│
├── platform/                         ← 平台抽象层
│   ├── include/platform/
│   │   ├── audio_output.h            ← 音频输出接口
│   │   ├── storage.h                 ← 采样数据读取接口
│   │   ├── input.h                   ← 控制输入接口
│   │   └── timer.h                   ← 时间/节拍接口
│   ├── sim/                          ← 桌面模拟器平台实现
│   │   ├── CMakeLists.txt
│   │   ├── audio_output_sdl.cpp
│   │   ├── storage_filesystem.cpp
│   │   ├── input_keyboard.cpp
│   │   └── timer_chrono.cpp
│   └── mcu/                          ← MCU 平台实现 (AT32F435)
│       ├── CMakeLists.txt
│       ├── audio_output_i2s.cpp
│       ├── storage_psram.cpp
│       ├── input_sbus.cpp
│       └── timer_hw.cpp
│
├── app/                              ← 应用入口
│   ├── sim/
│   │   ├── CMakeLists.txt
│   │   └── main.cpp                  ← 桌面模拟器入口
│   └── mcu/
│       ├── CMakeLists.txt
│       ├── main.cpp                  ← MCU 固件入口
│       ├── startup_at32f435.s
│       ├── linker.ld
│       └── system_config.h
│
├── tests/                            ← 测试
│   ├── CMakeLists.txt
│   ├── unit/                         ← 单元测试 (Google Test)
│   │   ├── test_crossfade.cpp
│   │   ├── test_sample_player.cpp
│   │   ├── test_transmission.cpp
│   │   ├── test_dsp.cpp
│   │   └── test_mixer.cpp
│   ├── integration/                  ← 集成测试 (生成 WAV 验证)
│   │   ├── test_engine_sweep.cpp
│   │   └── test_gear_shift.cpp
│   └── golden/                       ← 回归测试参考文件
│       └── README.md
│
├── tools/                            ← 离线工具 (Python)
│   ├── requirements.txt
│   ├── sample_converter.py           ← WAV → 项目内部格式
│   ├── vehicle_config_gen.py         ← 车辆配置生成器
│   ├── audio_analyzer.py             ← 采样频谱分析
│   └── README.md
│
├── assets/                           ← 音频资产 (用户自备)
│   ├── README.md                     ← 说明如何准备采样数据
│   └── example/                      ← 示例 (合成/自录音频)
│       ├── config.toml
│       └── *.wav
│
├── docs/                             ← 文档
│   ├── architecture.md               ← 本文档
│   ├── algorithm.md                  ← 算法详解
│   ├── hardware.md                   ← 硬件设计
│   ├── sampling_guide.md             ← 采样录制指南
│   └── legal.md                      ← 法律合规说明
│
├── scripts/                          ← 构建脚本
│   ├── build_sim.sh
│   ├── build_mcu.sh
│   └── run_tests.sh
│
└── third_party/                      ← 第三方依赖
    ├── googletest/                   ← git submodule
    └── SDL2/                         ← 系统库 / submodule
```

---

## 4. 核心模块设计

### 4.1 EngineVoice — 引擎声音合成器

```cpp
// core/include/core/engine_voice.h
class EngineVoice {
public:
    struct Params {
        float rpm;          // 当前转速 (归一化 0-1 或实际值)
        float throttle;     // 油门开度 0-1
        float load;         // 引擎负载 0-1
    };

    // 每帧调用，填充输出缓冲区
    void process(const Params& params, int16_t* output, size_t frames);

    // 加载车辆采样数据
    void load_vehicle(const VehicleConfig& config, IStorage& storage);

private:
    SampleLayer layers_onload_[MAX_LAYERS];   // 加油采样层
    SampleLayer layers_offload_[MAX_LAYERS];  // 松油采样层
    Crossfader crossfader_;
    BiquadFilter eq_;
};
```

### 4.2 Crossfade — 交叉淡入

```cpp
// 找到当前 RPM 对应的两个相邻层，计算混合比例
struct CrossfadeResult {
    uint8_t layer_lo;       // 低层索引
    uint8_t layer_hi;       // 高层索引
    float mix;              // 0.0 = 全低层, 1.0 = 全高层
};

CrossfadeResult compute_crossfade(float rpm, const float* layer_rpms, uint8_t num_layers);
```

### 4.3 Platform 接口

```cpp
// platform/include/platform/audio_output.h
class IAudioOutput {
public:
    virtual ~IAudioOutput() = default;
    virtual bool init(uint32_t sample_rate, uint16_t buffer_frames) = 0;
    virtual void start(AudioCallback callback, void* user_data) = 0;
    virtual void stop() = 0;
};

// platform/include/platform/storage.h
class IStorage {
public:
    virtual ~IStorage() = default;
    virtual bool read_samples(const char* path, int16_t* buf, size_t count) = 0;
    virtual size_t get_sample_count(const char* path) = 0;
};
```

---

## 5. 构建系统

### 5.1 CMake 双目标

```cmake
# 顶层 CMakeLists.txt
option(TARGET_PLATFORM "Target platform: sim or mcu" "sim")

add_subdirectory(core)          # 始终编译
add_subdirectory(platform/${TARGET_PLATFORM})
add_subdirectory(app/${TARGET_PLATFORM})

if(TARGET_PLATFORM STREQUAL "sim")
    enable_testing()
    add_subdirectory(tests)
endif()
```

### 5.2 构建命令

```bash
# 桌面模拟器 (开发/调试)
cmake -B build/sim -DTARGET_PLATFORM=sim
cmake --build build/sim

# MCU 固件 (交叉编译)
cmake -B build/mcu -DTARGET_PLATFORM=mcu \
      -DCMAKE_TOOLCHAIN_FILE=scripts/arm-none-eabi.cmake
cmake --build build/mcu

# 运行测试
cd build/sim && ctest --output-on-failure
```

---

## 6. 数据流

### 6.1 实时音频处理流水线

```
Input (SBUS/键盘)
    │
    ▼
┌─────────────┐
│ Transmission │ ← RPM, Gear, Load 计算
└──────┬──────┘
       │ rpm, throttle, load
       ▼
┌─────────────┐
│ EngineVoice  │ ← 多层交叉淡入 + Load 混合
└──────┬──────┘
       │ engine_samples[]
       ▼
┌─────────────┐
│   Mixer      │ ← + turbo + backfire + horn + ...
└──────┬──────┘
       │ mixed_output[]
       ▼
┌─────────────┐
│ AudioOutput  │ ← I2S DMA (mcu) / SDL callback (sim)
└─────────────┘
```

### 6.2 缓冲策略 (MCU)

```
SD 卡 ──(预加载)──→ PSRAM (8MB, 存放当前车辆全部采样)
                         │
                    (按需读取)
                         ▼
                   SRAM 双缓冲 (2 × 512 samples × 2 bytes = 2KB)
                         │
                    (DMA 传输)
                         ▼
                      I2S DAC → 扬声器
```

---

## 7. 车辆配置格式

```toml
# assets/vehicles/inline4_sport/config.toml
[vehicle]
name = "Inline-4 Sport"
cylinders = 4
rpm_idle = 800
rpm_redline = 8500
firing_order = [1, 3, 4, 2]

[layers]
# 每层: 文件名, 录制时的 RPM
onload = [
    { file = "onload_1000.wav", rpm = 1000 },
    { file = "onload_2500.wav", rpm = 2500 },
    { file = "onload_4000.wav", rpm = 4000 },
    { file = "onload_5500.wav", rpm = 5500 },
    { file = "onload_7000.wav", rpm = 7000 },
    { file = "onload_8500.wav", rpm = 8500 },
]
offload = [
    { file = "offload_1000.wav", rpm = 1000 },
    { file = "offload_3000.wav", rpm = 3000 },
    { file = "offload_5000.wav", rpm = 5000 },
    { file = "offload_7000.wav", rpm = 7000 },
]

[transmission]
type = "auto"           # auto / manual / sequential
gears = [3.82, 2.36, 1.68, 1.31, 1.00, 0.79]
final_drive = 3.73

[extras]
turbo = true
backfire = true
```

---

## 8. 测试策略

### 8.1 单元测试

| 模块 | 测试内容 | 验证方法 |
|------|---------|---------|
| Crossfade | 边界值 (RPM=0, RPM=max, 恰好在层边界) | 数值断言 |
| SamplePlayer | 循环点无 click、相位连续性 | 零交叉检测 |
| DSP/Biquad | 频率响应曲线 | 与 scipy 参考对比 |
| Mixer | 多通道混合不溢出 | 输出范围 [-32768, 32767] |
| Transmission | 换挡逻辑、RPM 跳变合理性 | 状态机断言 |

### 8.2 集成测试

- **RPM 扫频测试**: 从 idle 到 redline 线性扫频，生成 WAV，验证频谱连续无断裂
- **换挡测试**: 模拟升降档，验证 RPM 过渡平滑
- **回归测试**: 与 `tests/golden/*.wav` 做 bit-exact 或 SNR 对比

### 8.3 CI 流水线

```yaml
# .github/workflows/ci.yml
- build sim target
- run unit tests
- run integration tests (generate WAV)
- compare with golden references (SNR > 60dB)
```

---

## 9. 法律合规与版权

### 9.1 原则

> **本项目不包含、不分发任何受版权保护的第三方音频数据。**

### 9.2 音频采样来源合规策略

| 来源 | 合规性 | 使用方式 |
|------|--------|---------|
| ✅ 自行录制 | 完全合规 | 用户自己录制真实引擎声音 |
| ✅ CC0/Public Domain 采样 | 完全合规 | 如 Freesound.org CC0 素材 |
| ✅ 合成生成 | 完全合规 | 用 DSP 算法合成的测试音频 |
| ⚠️ 游戏提取 | **仅限个人学习** | 不得分发，不入仓库 |
| ❌ 商业音频库 | 需购买授权 | 遵循各库 EULA |

### 9.3 仓库规则

1. **`assets/` 目录不提交实际采样** — 仅包含 README 说明和合成示例
2. **`.gitignore` 排除所有大型音频文件** — `*.wav`, `*.bank`, `*.fsb`
3. **tools/ 中的提取脚本** — 仅提供技术实现，附带法律声明：
   ```
   # LEGAL NOTICE:
   # This tool is provided for personal/educational use only.
   # Extracting audio from commercial games may violate their EULA.
   # Users are solely responsible for ensuring compliance with
   # applicable licenses and copyright laws.
   # This project does NOT distribute any copyrighted audio data.
   ```
4. **采样录制指南** (`docs/sampling_guide.md`) — 指导用户如何合法获取采样：
   - 自行录制真实引擎（手机/录音笔 + 后期处理）
   - 使用 CC0 开放素材
   - 使用 DSP 合成

### 9.4 代码许可

- 本项目代码: **MIT License**
- 第三方依赖: 各自许可证（均为 MIT/BSD/zlib 兼容）
- 参考算法: 基于公开学术论文和开源实现，无专利风险

### 9.5 与原项目的关系

本项目是**独立的全新实现**，不是 Rc_Engine_Sound_ESP32 的 fork：
- 不复制原项目代码（算法完全重写）
- 不使用原项目的音频数据（.h 头文件中的 PCM 数据有版权）
- 仅参考其公开的架构思路（属于合理的逆向工程学习）
- 原项目 License: GPL-3.0 → 我们的独立实现使用 MIT，无传染性问题

---

## 10. 硬件规格

### 10.1 目标平台

| 组件 | 型号 | 规格 | 单价 |
|------|------|------|------|
| MCU | AT32F435RMT7 | 288MHz Cortex-M4F, 1MB Flash, 512KB SRAM | ¥8 |
| PSRAM | APS6404L-3SQR | 8MB QSPI, 133MHz | ¥4 |
| DAC | PCM5102A | 32-bit I2S, 384kHz, SNR 112dB | ¥3 |
| SD 卡座 | — | Micro SD, SPI/SDIO | ¥1 |
| 功放 | PAM8403 | 2×3W D 类 | ¥1 |
| 其他 | 电源/连接器/PCB | — | ¥8 |
| **合计** | | | **~¥25** |

### 10.2 资源预算

| 资源 | 可用 | 音频占用 | 余量 |
|------|------|---------|------|
| CPU | 288MHz | ~7MHz (2.4%) | 97% |
| SRAM | 512KB | 16KB DMA 缓冲 | 496KB |
| PSRAM | 8MB | 6MB 采样数据 | 2MB |
| PSRAM 带宽 | 15MB/s | 0.5MB/s | 97% |

---

## 11. 开发路线图

### Phase 1: 核心算法 (sim 验证)

- [ ] 搭建 CMake 项目骨架
- [ ] 实现 core/ 基础模块 (SamplePlayer, Crossfade, Mixer)
- [ ] sim 平台层 (SDL2 音频输出)
- [ ] 用合成正弦波验证交叉淡入
- [ ] 单元测试全覆盖

### Phase 2: 真实采样集成

- [ ] 采样录制/获取（合法来源）
- [ ] 车辆配置格式解析
- [ ] 完整 EngineVoice 流水线
- [ ] A/B 对比：变速率 vs 多层交叉淡入

### Phase 3: MCU 移植

- [ ] AT32F435 BSP 搭建
- [ ] I2S DMA 双缓冲
- [ ] PSRAM 驱动 + SD 预加载
- [ ] SBUS 输入解析
- [ ] 性能优化 (CMSIS-DSP, 定点化)

### Phase 4: 完善与开源

- [ ] 变速箱模拟
- [ ] 附加声道 (涡轮、回火、喇叭)
- [ ] PCB 设计 + 外壳
- [ ] 文档完善
- [ ] GitHub 开源发布

---

## 12. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| PSRAM 延迟尖峰 | 音频 glitch | SRAM 双缓冲隔离，提前预取 |
| 采样循环点 click | 音质下降 | 零交叉对齐 + 短交叉淡入 |
| AT32 QSPI 兼容性 | 无法驱动 PSRAM | 备选 STM32F446 + SPI PSRAM |
| 合法采样不足 | 车辆种类少 | 提供录制指南 + 合成引擎声 |
| 算法 CPU 超预算 | 实时性不足 | CMSIS-DSP 加速 / 降层数 |
