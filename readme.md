# AC-IR 固件源码（重制版）

ESP8266 原生 HomeKit 红外空调遥控器固件。本工程基于最新的 **IRremoteESP8266 v2.9.0**（2026-01-03 发布）全部重写。

> 原项目 [Mixiaoxiao/ESP8266-IR-HOMEKIT](https://github.com/Mixiaoxiao/ESP8266-IR-HOMEKIT)
> 仅发布二进制固件、未公开源码且 2021 年起停更。本工程依据其公开文档（功能清单、引脚定义、
> 更新日志）重新实现，功能对齐并做了升级。

## 功能

- **原生 HomeKit 空调配件**（HeaterCooler 服务，无需任何桥接器）
  - 开关、模式（自动/制冷/制热）、目标温度、风速、扫风
  - 配对码：`111-11-111`
- **红外发射**：基于 `IRac` 统一空调接口，支持 v2.9.0 全部空调协议（70+ 种）
- **协议自动识别**：网页一键进入识别模式，用原配遥控器按任意键即可识别协议并同步状态
- **红外反馈**：接收到与设定协议相同的遥控信号时，自动同步空调状态到 HomeKit
- **Web 控制页**：`http://<设备IP>/`，手机浏览器可用，支持电源/模式/温度/风速/扫风/灯光/静音/强劲/节能/滤网/清洁
- **WiFi 配网**：未联网时自动开启 `ESP_CONFIG_XXXXXX` 热点（ captive portal ），已联网时也可从网页重新配网
- **网页固件更新**：`http://<设备IP>/update`
- **状态掉电保存**：EEPROM 存储（协议/型号/全部空调状态），重启自动恢复
- **按键**：长按 5 秒重置 HomeKit 配对并重启


## 硬件连接

| 功能 | GPIO | NodeMCU 丝印 | 必需 |
|---|---|---|---|
| 红外发射 | IO14 | D5 | 是 |
| 红外接收 | IO12 | D6 | 否（协议识别/反馈需要） |
| WiFi 指示灯 | IO2 | D4（板载蓝灯，低电平亮） | 否 |
| 红外指示灯 | IO13 | D7 | 否 |
| 按键 | IO0 | D3（Flash 键，低电平有效） | 否 |

引脚可在 `src/config.h` 中修改。最简电路：红外发射管正极接 IO14、负极接 GND（遥控距离约 1 米，
加三极管驱动可大幅延长，详见本目录上一级 `readme.md` 与 `pcb/`）。

## 编译与烧录

> 不想自己编译：本仓库已附带编译好的固件 `../firmware/ESP8266_IR_20261001_4M.bin`
> （v2.0.0，IRremoteESP8266 v2.9.0，适用 4MB Flash 的 ESP-12E/F、NodeMCU、D1 mini），
> 直接用 `../tools/` 里的烧录工具刷到 **0x0** 地址即可。

### 1. 安装 PlatformIO

- 方式 A：VS Code 安装 PlatformIO IDE 插件（推荐）
- 方式 B：命令行 `pip install platformio`

### 2. 编译

```powershell
cd AC-IR/firmware-src
pio run
```

依赖库（`crankyoldgit/IRremoteESP8266@^2.9.0`、`mixiaoxiao/HomeKit-ESP8266@^1.2.0`）会自动下载。
产物：`.pio/build/nodemcuv2/firmware.bin`

### 3. 首次烧录（串口）

```powershell
pio run -t upload
```

或用乐鑫官方 Flash 下载工具 / `../tools/tasmotizer`，将 `firmware.bin` 烧录到 **0x0** 地址。

> **首次刷机或从旧固件迁移时，务必先整片擦除（Erase）再烧录**，避免残留旧的配对数据。

### 4. 后续升级（免串口）

设备联网后访问 `http://<设备IP>/update`，选择新编译的 `firmware.bin` 上传即可。

## 使用

### WiFi 配网

1. 设备未联网时自动开启热点 `ESP_CONFIG_XXXXXX`，连接后自动弹出配网页面（或访问 192.168.4.1）
2. 选择/输入 WiFi 名称与密码，保存后设备自动重启并联网
3. 板载蓝灯（IO2）：闪烁 = 未联网，熄灭 = 已联网

### 添加到 Apple 家庭

1. 保持 iPhone 与设备在同一局域网
2. 家庭 App → 添加配件 → 更多选项 → 选择 "AC IR"
3. 输入配对码 `111-11-111`

### 设置空调协议（关键步骤）

必须设置与实体遥控器匹配的**协议**和**子型号**，否则空调无法响应：

1. 浏览器访问 `http://<设备IP>/`
2. 点击「识别空调协议」，60 秒内用原配遥控器对准红外接收头按任意键
3. 页面显示识别到的协议后自动保存；识别失败可手动在协议列表中选择
4. 参考经验（与原固件一致）：
   - 格力 → 通常选 `KELVINATOR`
   - 美的 → 通常选 `COOLIX`
   - 海尔 → `HAIER_AC` / `HAIER_AC_YRW02` / `HAIER_AC176`
   - 海信 Hisense / Kelon（科龙）→ `KELON168`
   - 代工厂原因，品牌与协议不一定一一对应，以识别结果为准
5. **子型号**：下方「子型号」下拉框列出该协议的原装遥控器型号枚举，
   仅少数协议需要区分，不确定时保持「默认」即可。识别成功后会自动填写。

### 按键

- **长按 5 秒**：重置 HomeKit（清除全部配对信息）并重启

## HomeKit 与 Web 的功能差异

HomeKit 的 HeaterCooler 服务只支持 自动/制热/制冷 三种模式；**除湿、送风**模式以及
**静音、强劲、节能、灯光、滤网、清洁**等扩展项请使用 Web 控制页。

HomeKit 风速滑块映射：0=自动，1 ~ 29=Min，30 ~ 49=Low，50 ~ 69=Medium，70 ~ 89=High，90 ~ 100=Max。

## 常见问题

**Q: Home App 提示"配件无响应"？**
A: 确认 iPhone 与设备在同一 WiFi；在 Web 页「系统 → 重启设备」；仍不行则长按按键重置配对后重新添加。

**Q: 空调不响应红外指令？**
A: 90% 是协议/子型号不对，先用「识别空调协议」；其次检查发射管接线与供电（发射时电流大，
建议 5V1A 以上电源，PCB 上 C5 用电解/钽电容）。

**Q: 「识别空调协议」一直超时？**
A: 说明该协议未被识别（或固件未支持）。海信 Hisense / Kelon 机型请手动选 `KELON168`
（168bit 协议，普通 `KELON` 只能开机、不出风）。

**Q: 室温显示为什么不真实？**
A: 本设计无温度传感器，HomeKit 中"当前温度"回显的是设定温度。

**Q: 想要 1MB Flash 的 ESP-01 版本？**
A: 本固件含全部空调协议，体积约 700KB+，需 2MB 以上 Flash（推荐 ESP-12E/F 4MB）。
ESP-01（1MB）放不下完整版。

## 工程结构

```
firmware-src/
├── platformio.ini        # PlatformIO 配置（board=nodemcuv2 / ESP-12E）
├── lib/
│   └── HomeKit-ESP8266/  # HomeKit 库 v1.2.0（已内嵌并打补丁，见下）
└── src/
    ├── main.cpp          # 主流程：setup/loop、按键、WiFi 指示灯
    ├── config.h          # 引脚定义、配对码、超时等常量
    ├── settings.cpp/h    # EEPROM 状态持久化（带 CRC16 校验）
    ├── ir_ctrl.cpp/h     # IRac 发送、IRrecv 接收、协议识别、防抖发送
    ├── homekit_ac.cpp/h  # HomeKit 特征映射与双向同步
    ├── web_ui.cpp/h      # Web 控制页与表单处理、网页固件更新
    ├── wifi_portal.cpp/h # WiFi 连接、AP 配网门户（captive portal）
    └── my_accessory.c    # HomeKit 配件定义（HeaterCooler 服务）
```

> **关于内嵌的 HomeKit-ESP8266**：该库自带的 `base64.h`（C 函数）会遮蔽 ESP8266 core
> 的同名头文件（`ESP8266WebServer` 依赖其中的 C++ `base64` 类），导致编译失败。
> 因此将库（v1.2.0，MIT 协议）内嵌到 `lib/` 并把冲突文件改名为 `hk_base64.h/c`。
> 这样工程开箱即可编译，无需任何手动补丁。

## 体积与内存

编译结果（PlatformIO，ESP8266 core 3.1.x）：

- **Flash：741KB**（约 71% @ 1MB 程序分区）→ 需 2MB 以上 Flash，推荐 ESP-12E/F（4MB）
- **RAM：静态占用约 41.7KB**（51%），HomeKit 配对瞬间堆占用更高，实测需保证供电稳定

## 致谢

- [crankyoldgit/IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266) — 红外协议库（v2.9.0）
- [Mixiaoxiao/Arduino-HomeKit-ESP8266](https://github.com/Mixiaoxiao/Arduino-HomeKit-ESP8266) — 原生 HomeKit 协议栈
- [Mixiaoxiao/ESP8266-IR-HOMEKIT](https://github.com/Mixiaoxiao/ESP8266-IR-HOMEKIT) — 原项目（功能与硬件设计的参考来源）
