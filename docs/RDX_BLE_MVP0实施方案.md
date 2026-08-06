# AC710N RDX BLE 0 号 MVP 实施方案

> 文档状态：实施稿 v0.3（MVP0-A 代码与构建完成，实机联调待执行）
> 编写日期：2026-08-06
> 目标工程：AC710N / BR56 Type-C 有线耳机
> 参考工程：`t2616cc-firmware/`（JL701N / BR28）

## 1. 结论

0 号 MVP 建议只移植 RDX 的 BLE 接入闭环，不移植录音、文件、Wi-Fi、SPI、RTC、充电仓、LED、马达和 OTA 等业务。

MVP 的最终验收不是“手机系统显示 BLE 已连接”，而是：

1. 目标 APP 能发现 AC710N 设备；
2. APP 能建立 BLE/GATT 连接并完成服务发现；
3. APP 能读取设备身份、订阅通知，并完成进入设备页所必需的最小 RDX 握手；
4. APP 显示设备在线；
5. 上述过程不破坏现有 USB Audio、USB Mic 和 USB HID 功能。

建议把实现内部拆成 `MVP0-A` 和 `MVP0-B` 两个闸门，但二者共同构成 0 号 MVP：

- `MVP0-A`：广播、连接、GATT 服务、读写和 Notify 通路可用，可用通用 BLE 工具验证；
- `MVP0-B`：目标 APP 完成最小 RDX 身份识别或认证，APP 显示在线。

这样可以先排除 BR28 到 BR56 的 BLE 栈适配问题，再定位 APP 协议问题。

### 1.1 架构硬约束

本方案实施时必须同时满足以下两条要求，它们的优先级高于具体文件组织和移植便利性：

1. **RDX 完整模块化**：是否选择 RDX 第三方协议是唯一的用户配置入口。启用时才编译、链接和运行 RDX；关闭时不得残留 RDX 广播、GATT、任务、定时器、回调、VM、日志或协议符号；
2. **最小侵入 JL 原生平台**：RDX 业务不得直接散落到 PC/UAC、USB、音频、板级或电源流程。确需修改 JL 原生文件时，只增加受门控的通用生命周期钩子或第三方协议注册项，并纳入第 10 节的修改白名单。

`MVP0-A` 和 `MVP0-B` 只是验收里程碑，不是两个独立的产品配置开关，避免出现半启用组合。

### 1.2 当前实施进度（2026-08-06）

MVP0-A 的代码骨架已经落地：

- AC710N 使用未占用的 bit 17 定义 `RDX_EN`，并派生唯一总门控 `TCFG_RDX_ENABLE`；
- 配置源 `src/蓝牙配置.json` 与生成配置已同步开启 BLE、第三方协议并选择 RDX；
- PC/UAC 路径只增加通用 `multi_protocol_pc_start/stop`，`pc.c` 不包含任何 `rdx_*` 头文件、函数或业务判断；
- RDX 被隔离为 Entry、BR56 BLE Transport、Identity、MVP0 Core 和 Product Config，构建系统仅在总门控开启时加入 4 个 `.c` 文件；
- GATT Profile 已与 701 参考工程按字节核对一致，均为 387 bytes；
- RDX ON 和 RDX OFF 均已完成干净编译。OFF 产物的构建列表、Map、ELF 中 `rdx_*` 计数均为 0，也不存在 PC bridge 运行符号；
- ON 产物只包含 MVP0 RDX 对象和 BLE/Identity/Core 符号，不包含 701 的录音、文件、Wi-Fi、DUT、LED、按键、TWS 等全量 RDX 业务。

当前固件仍属于“可上板联调”的 MVP0-A，不等于 APP 已在线：产品身份暂时镜像 701 当前的 Zenchord Case 测试配置，协议版本暂定为 `0x01`，AuthKey 和 Label SN 为全零开发占位值。目标 APP 实机验证和 Golden Trace 尚未完成，MVP0-B 最小握手必须以实测数据为准。

## 2. 当前工程事实

### 2.1 AC710N 当前配置

当前 BR56 生成配置和两种目标状态如下：

| 配置项 | 当前值 | RDX ON | RDX OFF |
| --- | ---: | ---: | ---: |
| `TCFG_APP_PC_EN` | 1 | 保持 1 | 保持 1 |
| `TCFG_APP_BT_EN` | 0 | 保持 0 | 保持 0 |
| `TCFG_USER_BLE_ENABLE` | 1 | 1 | USB-only 产品可恢复 0；有其他 BLE 所有者时可保持 1 |
| `TCFG_THIRD_PARTY_PROTOCOLS_ENABLE` | 1 | 1 | 0 |
| `TCFG_THIRD_PARTY_PROTOCOLS_SEL` | `RDX_EN` | 选择 `RDX_EN` | 不选择 `RDX_EN` |
| `TCFG_BT_BLE_ADV_ENABLE` | 0 | 保持 0，由 RDX 生成广播 | 保持 0 |
| `TCFG_USB_SLAVE_AUDIO_SPK_ENABLE` | 1 | 保持 1 | 保持 1 |
| `TCFG_USB_SLAVE_AUDIO_MIC_ENABLE` | 1 | 保持 1 | 保持 1 |
| `TCFG_USB_SLAVE_HID_ENABLE` | 1 | 保持 1 | 保持 1 |

这里的 `TCFG_USER_BLE_ENABLE=0` 是当前 USB-only 产品的 RDX OFF 配置。若未来还有其他 BLE 功能，关闭 RDX 只移除由 RDX 引入的行为，不得误关其他模块合法拥有的 BLE 栈。

必须保持 `TCFG_APP_BT_EN=0`。打开完整 BT 应用会改变当前 Type-C 专用模式的判定和启动流程，也可能引入经典蓝牙、模式切换、音频焦点等无关变量。

### 2.2 BR56 已具备纯 BLE 启动能力

当前 `SDK/apps/common/config/bt_profile_config.c` 在 `TCFG_APP_BT_EN=0` 时，若第三方协议和 BLE 已启用，会把 `config_stack_modules` 配置为 `BT_BTSTACK_LE`。因此可以在 PC 模式中主动调用 `btstack_init()`，启动纯 BLE 栈，不需要开启 BT 应用模式。

这条路径是 MVP0 的基础：

```text
PC/UAC 模式启动
    ├── USB Device/UAC/HID 保持原流程
    └── RDX BLE bootstrap 调用 btstack_init()
            └── 收到 BT_STATUS_INIT_OK
                    ├── 初始化 app_ble 公共层
                    ├── 注册 RDX GATT profile
                    └── 开始 RDX 广播
```

### 2.3 701 与 710 的 BLE 接口有较高复用度

701 RDX BLE 服务使用的主要接口包括：

- `app_ble_hdl_alloc()`；
- `app_ble_profile_set()`；
- `app_ble_att_read_callback_register()`；
- `app_ble_att_write_callback_register()`；
- `app_ble_att_server_packet_handler_register()`；
- `app_ble_hci_event_callback_register()`；
- `app_ble_l2cap_packet_handler_register()`；
- `app_ble_set_adv_param()`、`app_ble_adv_data_set()`、`app_ble_rsp_data_set()`；
- `app_ble_adv_enable()`；
- `app_ble_att_send_data()`。

这些接口在当前 BR56 SDK 中均存在，所以 BLE 传输层不需要从控制器层重写。主要工作是初始化入口、产品身份、协议最小响应和业务依赖隔离。

### 2.4 不能照抄 `RDX_EN` 的位值

701 工程定义：

```c
#define RDX_EN (1 << 18)
```

但 710 工程的 bit 18 已被 `MULTI_CLIENT_EN` 使用。MVP0 必须在 710 的第三方协议位图中分配一个未占用位，并同步检查所有条件编译表达式、配置工具选项和在线调试重定义逻辑。

不能把 701 的 bit 18 原样复制到 710，否则会出现两个协议同时命中的隐蔽编译错误。

### 2.5 参考工程当前选中的是充电仓产品

参考工程 `rdx_app_config.h` 当前选择：

```c
#define RDX_AI_SEL_APP APP_ZENCHORD_EN
#define RDX_SEL_DEVICE DEVICE_ZENCORD_CC_T2616
```

`DEVICE_ZENCORD_CC_T2616` 是 Case/充电仓形态，对应的产品码、广播字段和 Read Characteristic 内容不一定适用于当前 Type-C 耳机。为先完成 BLE 通路联调，MVP0-A 暂时用隔离的开发配置镜像该身份；MVP0-B 前必须冻结目标 APP 所期待的产品身份，不能把这组占位配置直接转为量产配置。

### 2.6 单一 RDX 总门控

在 710 中分配唯一的 `RDX_EN` 位后，派生一个工程内统一使用的布尔总门控。示意如下，最终放置位置以配置头文件的可见范围为准：

```c
#if TCFG_THIRD_PARTY_PROTOCOLS_ENABLE && \
    (TCFG_THIRD_PARTY_PROTOCOLS_SEL & RDX_EN)
#define TCFG_RDX_ENABLE 1
#else
#define TCFG_RDX_ENABLE 0
#endif
```

配置工具仍以“启用第三方协议并选择 RDX”为用户入口，`TCFG_RDX_ENABLE` 只做派生，不再提供第二个人工开关。这里有意使用配置源宏 `TCFG_THIRD_PARTY_PROTOCOLS_SEL`，不使用可能被在线调试逻辑再次定义的 `THIRD_PARTY_PROTOCOLS_SEL`，确保 RDX 只由产品配置显式选择。若启用 `APP_ONLINE_DEBUG`，其协议位图重定义白名单也必须纳入 `RDX_EN`，但不得在产品未选择 RDX 时自动带入 RDX。所有 RDX 构建项、声明、注册、初始化、退出和业务代码统一检查 `TCFG_RDX_ENABLE`，不允许各文件重复拼接不同的条件表达式。

对不完整组合应在编译期直接报错，至少检查：

- 选择 `RDX_EN` 但未启用第三方协议；
- `TCFG_RDX_ENABLE=1` 但 `TCFG_USER_BLE_ENABLE=0`；
- MVP0 的 Type-C 配置中 `TCFG_RDX_ENABLE=1` 且 `TCFG_APP_BT_EN=1`。

## 3. MVP0 范围

### 3.1 范围内

- BR56 纯 BLE 栈随 PC 模式启动和退出；
- RDX 广播和扫描响应；
- 与 701 一致的 GATT 服务布局、UUID、属性和 Handle；
- BLE 连接、断开、MTU、CCC 订阅和 Notify；
- GAP 设备名读取；
- RDX 设备身份特征读取；
- 电量特征返回一个合法值；
- APP 进入设备页所必需的最小请求和响应；
- 必要的测试身份数据和最小 VM 持久化；
- BLE 与 USB Audio/Mic/HID 并行工作验证；
- 由 `TCFG_RDX_ENABLE` 统一控制的构建、注册、启动和退出闭环。

### 3.2 明确不在范围内

- 录音和录音状态同步；
- 文件列表、文件上传、批量数据传输；
- eMMC、SD、FATFS 和文件格式化；
- Wi-Fi、UART、ESP32、SPI；
- RTC 和时间同步业务；
- LED、马达、充电仓和耳机配对业务；
- OTA 实际升级能力；
- SPP 和经典蓝牙；
- TWS；
- 正式量产密钥烧录系统；
- RDX 全量命令集。

GATT 表可以保留 OTA Service，以维持与 701 相同的服务指纹，但 MVP0 中 OTA 写入必须明确拒绝或忽略，不能进入升级流程。

## 4. MVP0 验收标准

### 4.1 功能验收

1. 上电或插入 Type-C 后，目标 APP 在 5 秒内能发现设备；
2. APP 显示的设备名、产品类型和绑定状态符合冻结的目标身份；
3. APP 能完成连接、服务发现、身份读取和 Notify 订阅；
4. APP 显示设备在线，并能保持在设备页至少 10 分钟；
5. APP 主动断开后，设备恢复广播，可再次连接；
6. 连续执行 20 次连接/断开，无死机、无无法恢复广播；
7. 连续执行 10 次 Type-C 插拔和冷启动，BLE 与 USB 均能恢复；
8. 用通用 BLE 工具可以读到预期 GATT 表，并完成 RX Write / TX Notify 回环测试。

### 4.2 USB 共存验收

1. USB 枚举结果与移植前一致；
2. UAC 左右声道播放正常；
3. USB Mic 上行正常；
4. USB HID 按键正常；
5. BLE 扫描、连接、订阅和少量交互期间，USB 音频无持续卡顿、无重枚举；
6. USB 连续播放并开启 Mic 30 分钟，同时保持 APP 在线，无复位和明显音频异常。

### 4.3 日志验收

调试版本至少输出以下状态，正式版本可降级或关闭：

- BLE 栈初始化开始/完成；
- 广播开始/停止和失败码；
- 连接 Handle、MTU、断开原因；
- CCC 开启/关闭；
- RDX RX/TX 的长度、命令类型和脱敏十六进制摘要；
- APP-ready 状态；
- 不打印完整 AuthKey、AppKey 或其他正式密钥。

### 4.4 模块门控验收

必须分别干净编译和验证 RDX ON、RDX OFF 两种配置：

1. RDX ON：RDX 源文件和必要 BLE 模块被编译，APP-ready 与 USB 共存验收通过；
2. RDX OFF：Map 中不存在 `rdx_*` 代码、数据和库符号，不注册 RDX GATT、消息处理器、任务、定时器或回调；
3. RDX OFF：设备不产生 RDX 广播，不创建 RDX VM 项，不输出 RDX 日志，也不因 RDX 调用 `btstack_init()`；
4. RDX OFF：USB 枚举、Audio、Mic、HID、功耗和启动时序与当前 USB-only 基线一致；
5. 若其他功能独立启用 BLE，RDX OFF 不影响其工作，但固件中仍不得出现 RDX 行为和符号。

## 5. BLE 兼容合同

MVP0 不以“实现一个能连的自定义 BLE 服务”为目标，而是复刻 APP 依赖的 RDX BLE 外部合同。

### 5.1 广播

701 当前格式为：

- Advertising Data：Flags `0x0A` + Complete Local Name；
- Scan Response：Manufacturer Specific Data。

Manufacturer Data 的字段顺序为：

```text
PRODUCT_CODE
+ reversed BLE MAC
+ FACTORY_CODE
+ mode/bound byte
+ ability (4 bytes)
+ optional protocol version
+ PRODUCT_TYPE (2 bytes)
+ "NV"
```

以下字段必须以目标 APP 的实际规则冻结：

| 字段 | 说明 |
| --- | --- |
| `BLE_LOCAL_NAME` | APP 扫描页显示名和过滤条件之一 |
| `PRODUCT_CODE` | 产品形态标识，参考工程中 601/603 等含义不同 |
| `FACTORY_CODE` | 品牌或渠道标识 |
| `PRODUCT_TYPE` | 2 字节产品类型 |
| bound bit | 未绑定/已绑定状态，影响 APP 流程 |
| ability | APP 功能入口能力位图 |
| protocol version | 是否出现及其值必须与 APP 版本匹配 |
| BLE MAC | 字节序和持久性必须与 701 一致 |

### 5.2 GATT 服务

建议 MVP0 保持 701 的完整 GATT 表和 Handle 不变：

| 服务/特征 | UUID 或 Handle | MVP0 行为 |
| --- | --- | --- |
| GAP Device Name | `0x2A00`, value `0x0003` | 返回 RDX BLE 名称 |
| RDX Primary Service | `06068D0C-6B97-11EF-B864-0240AC120002` | 保留 |
| RDX RX | `06068D1C-6B97-11EF-B864-0241AC120002`, value `0x0006` | Write Without Response，送入最小协议处理器 |
| RDX TX | `06068D2C-6B97-11EF-B864-0242AC120002`, value `0x0008` | Notify，CCC `0x0009` |
| RDX Identity Read | `06068D3C-6B97-11EF-B864-0243AC120002`, value `0x000B` | 返回目标产品身份数据 |
| Battery Service | `0x180F` | 保留 |
| Battery Level | `0x2A19`, value `0x000E` | 返回 0～100，CCC `0x000F` |
| OTA Primary Service | `00239A6F-C616-89BB-3374-F05AF588A7B3` | 只保留服务指纹 |
| OTA RX/TX | value `0x0012` / `0x0014` | MVP0 不执行升级，CCC `0x0015` |

Handle 是否必须固定取决于 APP，但复用 701 profile 数据成本最低，也能减少未知兼容问题。

### 5.3 Identity Read

701 中不同产品形态返回的数据不同：

- 录音卡片/PIN 类产品通常返回 24 字节 AuthKey；
- Case 类产品会拼接本机 MAC、耳机 MAC、AuthKey 和 Wi-Fi MAC 等字段。

因此 Identity Read 不能在产品形态未确认前实现。它很可能是“系统 BLE 已连接但 APP 不认设备”的第一处差异。

### 5.4 安全和绑定

- 首版先复现 701 的配对、Bonding 和 Security Manager 参数；
- 每次改变 MAC、AuthKey、绑定位或安全参数后，手机端和设备端都要清除旧 Bond；
- MVP0 可以使用受控测试身份，但必须通过单独配置文件或 VM 注入，不能散落硬编码在 BLE 回调中；
- 正式密钥和量产烧录不属于 MVP0，但接口边界要预留。

## 6. 推荐软件结构

建议新建一个小型、可替换的 RDX 模块，不直接把 701 的整个 `rdx_protocol/` 目录复制到 710。对外只暴露生命周期入口，BLE 传输、身份和 MVP0 协议实现均为模块内部依赖：

```text
SDK/apps/common/third_party_profile/rdx_protocol/
├── rdx_protocol_entry.c     # 唯一外部生命周期入口，幂等 start/stop
├── rdx_protocol_entry.h
├── rdx_ble_transport_br56.c # BR56 Handle、广播、GATT 回调、连接和 RX/TX
├── rdx_ble_transport_br56.h
├── rdx_identity.c           # 产品码、厂商码、名称、MAC、AuthKey、绑定位
├── rdx_identity.h
├── rdx_mvp0_core.c          # APP-ready 所需的最小请求/响应状态机
├── rdx_mvp0_core.h
└── rdx_product_config.h     # 测试身份和产品配置，不保存量产正式密钥
```

模块边界如下：

```text
PC 模式
  └── 通用 third-party PC start/stop hook
        └── JL 第三方协议公共层 / BLE bootstrap
              └── RDX 生命周期入口（仅 TCFG_RDX_ENABLE）
                    └── BR56 BLE transport
                          ├── 广播/扫描响应
                          ├── GATT Profile
                          └── RX / TX
                                └── MVP0 协议核心
                                      └── 身份配置/VM
```

依赖方向必须保持单向：PC、USB、UAC、音频和板级模块只认识通用第三方协议生命周期，不包含 `rdx_*` 调用；第三方协议统一分发点只允许出现一个受 `TCFG_RDX_ENABLE` 保护的 RDX 生命周期注册；RDX 可以调用 JL 公共 API，但 JL 核心业务模块不得反向包含 RDX 头文件。

这样做有四个直接结果：

1. MVP0 不会链接录音、文件、Wi-Fi、SPI 等硬件依赖；
2. 后续接入完整 RDX 协议库时，只替换 `rdx_mvp0_core`，BLE transport 和生命周期入口继续复用；
3. RDX OFF 时整个目录可从构建列表消失，不留下空壳任务和回调；
4. JL 原生流程只保留通用钩子，不感知 RDX 广播、命令和产品身份。

## 7. 初始化和退出设计

### 7.1 启动时序

推荐时序：

1. `pc_mode_init()` 保持现有 USB 启动逻辑；
2. 在 `pc_task_start()` 发出 `USBSTACK_START` 请求后，只调用通用 `multi_protocol_pc_start()`，`pc.c` 不包含任何 RDX 头文件或函数名；
3. 通用 PC bridge 根据第三方协议配置决定是否幂等调用 `btstack_init()`，并记录该 bridge 是否拥有本次 BLE 栈生命周期；
4. 通用 PC bridge 使用 `APP_MSG_HANDLER` 监听 `MSG_FROM_BT_STACK`；
5. 收到 `BT_STATUS_INIT_OK` 后调用 JL 第三方协议公共初始化，再由统一分发点在 `TCFG_RDX_ENABLE` 下进入 RDX 生命周期入口；
6. BLE 公共层初始化顺序建议保持 SDK 现有第三方协议模式：
   - `le_device_db_init()`；
   - `app_ble_sm_init()`；
   - `app_ble_init()`；
   - `ble_op_multi_att_send_init()`；
   - `app_ble_hdl_alloc()`；
   - 设置 MAC、Profile 和回调；
   - 设置广播参数、Advertising Data、Scan Response；
   - 开启广播。

不得在 `pc_mode_init()` 中同步等待 BLE 初始化完成，避免延迟 USB 枚举。除通用 `multi_protocol_pc_start()` 调用外，不得改写 PC/UAC 原启动时序。

### 7.2 断开处理

- 保存断开原因；
- 清空连接 Handle、MTU、CCC 和 APP-ready 状态；
- 停止未完成的最小协议事务；
- 在允许的断开原因下自动恢复广播；
- 不执行 701 中的录音恢复、文件传输清理、Wi-Fi、电源或 LED 业务。

### 7.3 退出处理

PC 模式退出或关机时：

1. 停止广播；
2. 主动断开连接；
3. 释放 RDX BLE Handle；
4. RDX 生命周期入口完成自身资源退出；
5. `pc_task_stop()` 只调用通用 `multi_protocol_pc_stop()`；
6. 通用 bridge 调用第三方协议公共退出，由公共退出流程依次释放 RDX 和 `app_ble` 资源，并仅在自己拥有栈生命周期时调用 `btstack_exit()`；
7. 清空初始化标志，允许下一次启动。

所有入口和出口都必须幂等，避免模式消息或电源事件重复触发。`pc.c` 的 start/stop 两个通用钩子是本方案允许的核心路径最小改动，不得在其他 USB 或音频文件继续增加 RDX 分支。

## 8. 实施步骤

### 阶段 0：冻结 APP 外部合同

在 701 参考板上，用目标 APP 采集一份 Golden Trace：

- APP 名称、包名、版本、手机型号和系统版本；
- 完整 Advertising Data 和 Scan Response；
- BLE 地址类型和 MAC；
- GATT Service/Characteristic/Descriptor 列表；
- 连接后的 MTU；
- APP 读取的 Handle 和返回数据；
- APP 写入的前 5～10 个数据包；
- 设备发出的前 5～10 个 Notify；
- APP 显示在线的准确时间点；
- 未绑定和已绑定各一份流程，如果 MVP0 需要支持绑定。

产出：`RDX_BLE_Golden_Trace.md` 和脱敏十六进制日志。

没有 Golden Trace 也可以先完成 MVP0-A，但无法可靠判定 MVP0-B 的协议最小集。

### 阶段 1：配置和编译骨架

1. 在 710 的协议位图中为 `RDX_EN` 分配唯一位；
2. 定义只由第三方协议配置派生的 `TCFG_RDX_ENABLE`，增加非法组合的编译期报错；
3. 在 `src/蓝牙配置.json` 的第三方协议选项中增加 RDX，并通过 `project.jlproj` 和杰理配置工具生成 RDX ON/OFF 两套配置；
4. 保持 PC 模式、UAC、Mic、HID 配置不变，保持 SDK 默认 BLE 广播关闭；
5. 在 `SDK/build/genFileList.c` 中只按 `TCFG_RDX_ENABLE` 加入 RDX 源文件，不允许 RDX OFF 时依靠链接器回收；
6. 在第三方协议公共层增加 RDX 生命周期注册，在 PC 模式只增加通用 `multi_protocol_pc_start/stop` 钩子；
7. 确认 `bt_profile_cfg.h` 没有因为 RDX 而强制打开 SPP、A2DP、HFP、TWS 或完整 BT 应用；
8. 分别完成 RDX ON 和 RDX OFF 的干净编译、Map 检查及配置一致性检查；
9. 将实际修改文件与第 10 节白名单比对，超出白名单必须重新评审。

阶段验收：RDX ON 能链接且只引入纯 BLE 和 MVP0 模块；RDX OFF 的 Map 中不存在 `rdx_*` 符号，固件恢复当前 USB-only 行为。

### 阶段 2：完成 MVP0-A

1. 通过通用 PC third-party bridge 增加纯 BLE bootstrap；
2. 注册与 701 一致的 GATT Profile；
3. 生成目标身份对应的广播和扫描响应；
4. 实现 GAP Name、Identity 和 Battery Read；
5. 实现 CCC、RX Write、TX Notify；
6. 实现连接、断开和重新广播；
7. 用通用 BLE 工具完成验证。

阶段验收：不依赖 RDX 协议库，手机能稳定连接，RX/TX 可做固定测试回环。

### 阶段 3：完成 MVP0-B

按 Golden Trace 逐步增加最小状态机：

```text
ADVERTISING
    → GATT_CONNECTED
    → IDENTITY_READ
    → CCC_READY
    → MIN_HANDSHAKE_RX
    → MIN_HANDSHAKE_TX
    → APP_READY
```

每次只增加一个 APP 必需响应，并记录 APP 行为变化。APP 显示在线后停止扩展命令集，不进入录音、文件、Wi-Fi 或 OTA 业务。

如果 APP 只依赖广播、Identity Read 和 CCC，MVP0-B 不需要链接 `librdxApp.a`。

如果 APP 要求加密认证或 RDX 包级协议，则进入第 9 节的协议库决策闸门。

### 阶段 4：USB 共存与稳定性

- 在 USB 播放、Mic 上行和 HID 操作期间重复连接 APP；
- 记录 BLE 连接参数、CPU 占用、时钟和音频异常；
- MVP0 不移植 701 的批量传输连接参数管理器；
- 若只是身份握手，优先采用普通、低负载的 BLE 连接参数；
- 只有 Golden Trace 证明 APP 强制依赖时，才增加 PHY 或连接参数请求。

## 9. RDX 协议库决策闸门

参考工程的 `librdxApp.a` 包含：

- `rdx_encryption.c.o`；
- `rdx_protocol.c.o`；
- `rdx_queue.c.o`；
- `rdx_uxfile.c.o`；
- `xxpUart.c.o`。

701 和 710 都使用 PI32v2、`mcpu=r3`，所以该库存在复用可能；但不能据此认定二进制兼容：

- 701 使用 BR28 平台和 `r3-large` 构建选项；
- 710 使用 BR56 平台和普通 `r3` 库目录；
- `rdx_protocol.c.o` 对录音、文件、Wi-Fi、SPI、VM、电源等符号有大量外部依赖；
- 一旦调用 `rdx_protocol_task_create()`，可能把非 MVP 业务依赖带入链接；
- 701 的 `rdx_app_all_init()` 会初始化 VM、录音任务、RTC、SPI、eMMC/Wi-Fi 电源、LED、马达等，严禁在 MVP0 中直接调用。

决策顺序：

1. APP 不需要包级握手：不链接库；
2. APP 只需少量、已知且无加密的命令：在 `rdx_mvp0_core.c` 实现最小状态机；
3. APP 需要现有加密/认证：先做 `librdxApp.a` 的最小链接实验；
4. 链接实验引入大量业务符号：向 RDX 协议提供方申请 BR56/AC710N 版本的核心库，或申请把 transport/core 与 uxfile/uart 分库；
5. 只有最小核心库可用后，才把协议任务接入 MVP0 BLE transport。

建议的理想库边界：

```text
librdx_core.a       # framing、命令、认证、加解密
librdx_storage.a    # 文件和本地存储，MVP0 不链接
librdx_wifi.a       # Wi-Fi/UART，MVP0 不链接
```

## 10. JL 原生修改白名单

以下是 MVP0 允许的修改面。它既是预计修改点，也是代码评审白名单；实际实现超出此表时必须说明原因并单独评审，不能以移植方便为由继续扩散。

| 文件/目录 | 允许的最小修改 |
| --- | --- |
| `src/蓝牙配置.json` | 增加 RDX 选项和 RDX ON/OFF 配置依赖，作为配置源 |
| `SDK/apps/earphone/board/br56/sdk_config.h` | 仅接受配置工具生成结果，禁止单独手改 |
| `SDK/apps/earphone/include/app_config.h` | 分配 `RDX_EN` 唯一位，派生 `TCFG_RDX_ENABLE`，增加非法组合检查，并同步在线调试位图重定义逻辑 |
| `SDK/apps/common/third_party_profile/multi_protocol_main.[ch]` | 增加通用 PC 生命周期接口和一个受门控的 RDX 生命周期注册点 |
| `SDK/apps/common/third_party_profile/multi_protocol_event.c` | 将 RDX 纳入第三方协议公共状态回调的编译条件，不增加 RDX 业务逻辑 |
| `SDK/apps/earphone/mode/pc/pc.c` | USB 栈启动后和 PC 停止时各调用一个通用 third-party hook；不得包含 RDX 头文件或业务判断 |
| `SDK/build/genFileList.c` | 按 `TCFG_RDX_ENABLE` 加入 RDX 文件；RDX OFF 时不进入编译列表 |
| `SDK/apps/common/config/bt_profile_config.c` | 在 `TCFG_APP_BT_EN=0` 的通用纯 LE 分支补齐 btstack 已引用的 `hci_inquiry_support=0` 平台能力常量 |
| `SDK/apps/common/third_party_profile/rdx_protocol/` | 新增全部 RDX BLE、Identity、MVP0 core 和产品配置实现 |

`SDK/apps/common/config/include/bt_profile_cfg.h` 保持只审计、不修改。`bt_profile_config.c` 的实际修改仅补齐纯 LE 链接所需的关闭态能力常量，不为 RDX 打开 SPP、经典蓝牙或完整 BT 应用。

USB 描述符、`task_pc.c`、`usb_device.c`、`uac_stream.c`、USB Audio/Mic/HID、音频流程、板级驱动和电源流程均不在修改白名单内。

## 11. 风险与对策

| 风险 | 表现 | 对策 |
| --- | --- | --- |
| 把 BLE Link 当成 APP 在线 | 系统显示已连接，APP 仍报不支持或立即断开 | 把 APP-ready 作为最终验收，先采 Golden Trace |
| 产品身份选择错误 | APP 扫描不到、分类错误、身份读取失败 | 先冻结 APP、产品码、厂商码、产品类型和 AuthKey 规则 |
| `RDX_EN` 位冲突 | 错误模块被编译、行为随机 | 710 独立分配位；禁止复制 701 的 bit 18 |
| 多个 RDX 开关状态不一致 | 半编译、半初始化或关闭后仍有行为 | 只由第三方协议选择派生 `TCFG_RDX_ENABLE`，MVP0-A/B 不设产品开关 |
| RDX 业务侵入 `pc.c` 或 USB 路径 | 后续关闭/替换协议困难，影响 USB 稳定性 | PC 路径只调用通用 start/stop hook，RDX 逻辑集中在第三方协议目录 |
| RDX OFF 仍有符号或注册项 | 固件体积、功耗或运行行为没有恢复基线 | 构建列表和全部注册点统一门控，执行 OFF Map 与运行验收 |
| 误开完整 BT 模式 | Type-C 专用模式失效或出现经典蓝牙行为 | 保持 `TCFG_APP_BT_EN=0`，使用 BR56 的纯 LE 配置 |
| 默认广播与 RDX 广播冲突 | 广播内容变化、Handle 归属错误 | 保持 `TCFG_BT_BLE_ADV_ENABLE=0`，只由 RDX Handle 广播 |
| 直接调用 `rdx_app_all_init()` | GPIO、SPI、录音、Wi-Fi 等误动作或链接爆炸 | MVP0 使用独立小模块，禁止调用全量业务初始化 |
| 701 静态库 ABI/依赖不兼容 | 链接失败、运行异常、引入大量桩函数 | 协议库设置决策闸门，优先获取 BR56 core 库 |
| BLE 抢占影响 USB 音频 | 播放/Mic 卡顿或 USB 重枚举 | MVP0 不做批量传输，控制日志和连接参数，执行并行压力测试 |
| MAC/绑定缓存不一致 | 同一手机无法重连或认证异常 | 固定地址策略，每次身份变更时双端清 Bond |
| 测试密钥泄露 | 日志或源码包含正式密钥 | 只用测试身份，日志脱敏，正式烧录后移 |

## 12. 回退设计

### 12.1 唯一配置入口

产品配置只表达“第三方协议是否启用、是否选择 RDX”，代码统一使用第 2.6 节派生的总门控：

```c
#if TCFG_RDX_ENABLE
/* RDX source registration, lifecycle and business code */
#endif
```

不得新增 `RDX_BLE_MVP0_ENABLE` 等平行的用户开关。后续增加录音、文件或 OTA 等 RDX 子业务时，可以定义子功能宏，但必须满足：

```c
#if TCFG_RDX_ENABLE && TCFG_RDX_xxx_ENABLE
/* optional RDX sub-feature */
#endif
```

任何 RDX 子功能都不得绕过 `TCFG_RDX_ENABLE` 单独编译或运行。

### 12.2 RDX OFF 零残留

当前 Type-C 产品关闭 RDX 后：

- 构建系统不编译、不链接 RDX 目录及 RDX 静态库；
- 第三方协议分发点不引用 RDX 生命周期，Map 中不存在 `rdx_*` 符号；
- PC 通用 hook 不因 RDX 调用 `btstack_init()`；
- 不存在 RDX 广播、GATT、任务、定时器、回调、消息处理器、VM 和日志；
- USB、音频、板级和电源配置无需回滚，固件恢复当前 USB-only 行为。

如果未来其他功能拥有 BLE 栈，RDX OFF 只撤销 RDX 对栈的使用权，不负责关闭其他模块仍在使用的 BLE 资源。

### 12.3 构建回归矩阵

| 构建配置 | 预期结果 |
| --- | --- |
| 第三方协议 OFF、RDX 未选择、BLE OFF | 当前 USB-only 基线，Map 无 RDX |
| 第三方协议 ON、选择 RDX、BLE ON | MVP0 正常构建和运行 |
| 选择 RDX 但第三方协议或 BLE 未开启 | 配置工具禁止或编译期明确报错 |
| 第三方协议选择其他协议、RDX OFF | 其他协议不受影响，Map 和运行时均无 RDX |

## 13. 交付物

0 号 MVP 完成时应包含：

1. 本实施方案的评审定稿；
2. 701 Golden Trace 和测试环境记录；
3. BR56 MVP0 源码和配置工具源配置；
4. RDX ON/OFF 两套干净编译日志、Map 和大小变化；
5. 通用 BLE 工具验证记录；
6. 目标 APP 的连接/在线验证记录；
7. USB Audio/Mic/HID 共存验证记录；
8. 已知限制清单；
9. JL 原生修改白名单核对结果；
10. 下一阶段全量 RDX 业务移植的依赖清单。

## 14. MVP0-B 联调前需要确认的输入

进入 APP 最小握手编码和实机验收前，需要产品或 APP 侧确认以下信息：

- 目标 APP 名称、Android/iOS 包名和版本；
- 当前 710 产品应在 APP 中表现为耳机、录音卡片还是 Case；
- 应使用哪组 `BLE_LOCAL_NAME`、`PRODUCT_CODE`、`FACTORY_CODE` 和 `PRODUCT_TYPE`；
- 目标 APP 是否要求未绑定、绑定或两种状态；
- 可用于开发的测试 AuthKey/AppKey 或合法测试身份；
- 701 参考板是否能正常连接同一版本 APP；
- “连接成功”的产品口径：进入设备页、显示在线，还是还要完成某一条最小命令。

## 15. 粗略工作量

在目标身份明确、701 可正常连接且不需要全量加密协议库的前提下：

| 工作 | 预计时间 |
| --- | ---: |
| 701 Golden Trace 和身份冻结 | 0.5～1 天 |
| BR56 配置、编译和纯 BLE bootstrap | 1 天 |
| 广播、GATT、读写、Notify，即 MVP0-A | 1～2 天 |
| APP 最小握手，即 MVP0-B | 1～3 天 |
| USB 共存和稳定性验证 | 1 天 |

理想情况下约 4～7 个工程日。若 APP 必须依赖现有 `librdxApp.a` 的认证协议，而该库不能在 BR56 上最小化链接，则工期取决于核心库拆分或 BR56 版本交付，不应在 MVP0 中用大量空桩函数强行推进。
