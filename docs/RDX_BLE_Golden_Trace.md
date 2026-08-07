# RDX BLE Golden Trace

> 状态：v1.0，MVP0-B 最小 APP 外部合同已完成 710 实机验收
> 用途：限定 AC710N MVP0-B 的最小 APP 外部合同

## 1. 测试环境

| 项目 | 当前记录 |
| --- | --- |
| 参考硬件 | JL701N / BR28，Zenchord Case 配置 |
| 参考日志 | `参考工程的连接日志.log`（仅本地保存，包含敏感身份数据，不提交） |
| 710 最终复验日志 | `COM3_2026-08-07_13-41-10.log`（仅本地保存，不提交） |
| APP 名称、包名、版本 | 待补充 |
| 手机型号、系统版本 | 待补充 |
| 绑定状态 | MVP0 使用未绑定开发占位身份；已绑定流程待后续验证 |

## 2. BLE 外部合同

- Local Name：`Zenchord Case <AuthKey 后四位>`；开发占位身份对应 `Zenchord Case 0000`。
- Manufacturer Data：`603 + reversed BLE MAC + ZENCHD + bound/ability + V24 + E1 + NV`。
- 协议版本：十进制 24，即 `0x18`。
- GATT Profile：与 701 的 387-byte profile 完全一致。
- Identity Read：Case 形态的逗号分隔身份串；AuthKey、Label SN 和各 MAC 均按敏感数据处理，不在本文记录明文。
- 参考连接 MTU 日志：`ATT MTU = 514`（参考代码记录的是扣除 ATT 头后的发送 MTU）。

## 3. 已确认握手时序

```text
BLE connected
  -> Identity Read (首次发现或缓存失效时；后续连接可省略)
  -> TX CCC = 0x0001
  -> APP: *APP#battery#
  <- DEV: *DEV#battery#<legacy-C>#<right>#<legacy-L>#
  -> APP: *APP#version#
  <- DEV: *DEV#version#<hardware>#<firmware>#
  -> APP: *APP#appkey# + 32-byte payload + '#'
  <- DEV: *DEV#appkey#<result>#
  -> APP: *APP#rtc#<timestamp>#
  <- DEV: *DEV#rtc#<result>#<timestamp>#
  -> APP: *APP#ostype#<type>#
  <- DEV: *DEV#ostype#
```

参考日志中的请求顺序为 `battery -> version -> appkey -> rtc -> ostype`。`appkey` 请求总长度为 45 bytes，其中命令前缀 12 bytes、加密载荷 32 bytes、尾部分隔符 1 byte。Identity Read 是可缓存的身份发现步骤：首次发现或缓存失效时可能出现，但不作为每次连接进入认证流程的硬条件。

电量存在 701 历史线序：参考日志按结构字段打印 `L=100, R=0, C=0`，但 `rdx_app.c` 调用协议接口时实际按 `C, R, L` 传参，因此线上应答为 `*DEV#battery#0#0#100#`。MVP0-B 镜像线上合同，不按日志标签重排。版本接口同样按参考协议的 `hardware -> firmware` 顺序返回，即当前测试值为 `*DEV#version#0.0.1#1.0.0#`。

## 4. AppKey 校验合同

1. 取 AppKey 载荷前 16 bytes 作为解密 key。
2. 解密后 16 bytes，得到 16-byte 产品 AppKey。
3. 解密一次，并与官方 Zenchord 正式兼容 AppKey 集合比较：`14F6F7A1508E4155`、`385FA36EC106DE3D`、`7C0BAF778B727175`。
4. 成功返回 `*DEV#appkey#0#` 并进入 `APP-ready`。
5. 失败返回 `*DEV#appkey#1#` 并主动断开。

受 MAC 白名单限制的测试 Key `93C0C31635436482` 不在 MVP0 无条件接受范围内。实现直接使用参考工程的原始官方 `librdxApp.a`，但只引用 AppKey 解密接口；静态链接结果必须仅抽取 `rdx_encryption.c.o`，不得抽取协议、文件或 UART 对象，也不得打印解密后的明文 AppKey。

## 5. 已知限制与待采集项

- 710 的 AuthKey 和 Label SN 仍是全零开发占位值，尚未建立量产身份注入方案。
- RTC 不属于 MVP0 业务范围；当前候选实现为兼容 APP 握手返回成功回执并原样带回时间戳，但不修改或持久化系统 RTC。
- 710 首次 APP-ready 联调发现平台 `snprintf("%.*s")` 会把请求尾部分隔符一并复制，形成双 `#`；实现已改为定长按字节拼包，最终复验确认回执严格保持 `*DEV#rtc#0#<timestamp>#`。
- Battery 仍返回固定 `100%`，未接入真实电量采样。
- 尚缺 APP 名称、包名、版本、手机型号、系统版本和 APP 显示在线的准确时间点。
- 已取得目标 APP 在 710 上的未绑定最小握手联调日志；已绑定、换机和清除缓存后的完整流程仍待后续验证。

## 6. MVP0-B 实机验收结果

最终复验日志已经出现：

```text
[RDX] tx ccc=1
[RDX] rx ...
[RDX] APP-ready
```

首次发现或 APP 清除缓存后还应能看到 `[RDX] identity read`；常规重连未出现该日志属于允许行为，不影响 MVP0-B 判定。

最终复验中第一次连接以 `0x3E` 短暂断开，设备自动恢复广播，第二次连接后成功完成 MTU、TX CCC、Battery、Version、AppKey、RTC 和 OS Type 时序；AppKey 返回 `*DEV#appkey#0#`，RTC 返回 `*DEV#rtc#0#1786081300#`，随后 APP 进入设备页并显示在线。后续 `len=18/12/13` 的未响应请求属于录音等范围外业务，不影响本阶段结论。

MVP0-B 实机验收结论：**通过**。10 分钟在线、连续连接/断开、Type-C 插拔和 USB Audio/Mic/HID 共存属于实施方案阶段 4，需单独形成稳定性验收记录。
