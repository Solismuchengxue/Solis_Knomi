# WebUI 单管理员认证维护

## 产品流程

首次没有认证命名空间时，WebUI 开放。原「设置」页面新增独立的管理密码表单；用户名固定为 `admin`，密码要求 8–128 UTF-8 字节。保存后页面重新加载，使用浏览器自己的 Basic 登录框输入新密码。之后该表单只对已认证管理员开放，可修改密码。

没有管理员凭据配置文件、初始化脚本、编译密码或公开固定密码。设备仅在 NVS 的 `knomi_auth` 命名空间保存一个版本化的盐/校验值 blob，不写入 LittleFS/config.json，不在认证状态、保存响应或日志回显密码。

统一入口保护现有页面、API、`/fs/*`、OTA、404/405 回调和 WebSocket 升级。改密成功后旧密码失效；原有 WebSocket 的认证代次立即失效并被关闭，异步日志发送前再次核验，避免 fd 复用造成未认证接收。

## 边界

- HTTP Basic 不是 TLS。只用于可信局域网，不向公网开放；未设密时的开放窗口是明确的首次使用行为。
- 只有认证命名空间确实不存在才视为首次使用。已有命名空间但记录缺失、损坏、类型错误或应用可见 NVS 读取失败时关闭访问，不自动退回开放模式。
- 整个 NVS 被擦除后与新设备不可区分。固定 Arduino 框架也可能在特定 NVS 初始化错误下自动擦除并恢复 NVS；此时需要在隔离/可信网络重新设密。
- 不提供匿名重置、退出至开放模式或多用户功能。忘记密码需要另行授权本地维护；仅刷同一固件不会自动清除 NVS。
- 现有 Wi-Fi 显示/日志、主题、OTA 业务和原始诊断转储逻辑未改。原始 coredump 可能包含崩溃时的瞬时请求内存，应按敏感材料处理，不能声称任意转储都不含秘密。

## 本地验证和构建

使用项目约定的 PlatformIO Python 运行：

```powershell
python -m unittest discover -s firmware/DiverOfDark_KNOMI/tests -v
pwsh -File firmware/DiverOfDark_KNOMI/scripts/Build-AuthCandidate.ps1 -BuildWebUi
```

`python` 应替换为本机已核验的 PlatformIO 专用解释器；构建脚本提供 `-PlatformIOPython` 参数。脚本默认使用该主机登记的 PlatformIO Python 和 Node/npm，不自动安装前端包。`intelhex` 是根 README 已声明的 esptool 构建依赖。

候选只构建 `knomiv1-4mb`，输出到 `firmware/DiverOfDark_KNOMI/.pio/build-basic-auth-20261122/knomiv1-4mb/`。日志和 manifest 位于同级 `.pio/http-basic-auth-review-20261122/`。更新 WebUI 前保留并核验旧 `knomiWebpage.h`，不覆盖旧构建目录、烧录工具固件或 release 包。

新取源码需要同版 Release 的 `generated-assets.zip`：解压到固件根目录，提供 `src/generated/` 下 4 个未跟踪构建输入。只编译固件可省略 `-BuildWebUi`；仅修改 WebUI 时才重新打包该资源。不直接运行会重建全部生成目录的图片生成目标来覆盖现有输入。

认证依赖固定 ESP-IDF 4.4.7 的握手调用顺序，SDK 变更会停止编译。链接后再次反汇编核对 `httpd_uri` 确实调用握手前认证包装器，避免只在 HTTP 101 之后检查凭据。

## 部署、验收和恢复

1. 必须由打印机协调聊天确认打印结束并批准操作窗口；本地构建命令不连接或操作设备。
2. 通过现有 WebUI 的固件更新功能选择候选 `firmware.bin`，不是文件系统镜像；不改分区，不刷 bootloader，不清空 NVS。设备重启属于该授权窗口的实际部署动作。
3. 首次打开设置页设密；验证页面、只读状态/文件入口及 WS 的未认证请求返回 `401` 和 Basic challenge，不提前返回 WS `101`。验证正确/错误凭据、改密后旧密码/旧 WS 失效、重启后密码仍有效。
4. 已登录后人工检查设置、主题浏览、日志 WS 和固件更新界面仍可使用；本地构建/测试不替代真实浏览器和硬件验收。
5. 需要恢复时，优先在已认证且仍可用的 WebUI 上传 manifest 记录的旧固件。WebUI 不可用时，另行批准 USB 恢复并使用现有已保留发行/烧录材料；不要自行全片擦除。旧固件不包含本次保护，恢复后其原有开放行为也会恢复。

v1.1.0 保留已部署验收的固件字节。页脚元数据仍沿用项目关闭自动生成时保留的上游 `f052bef` 标识，并非 Solis 发布版本；发布身份以 Release 标签、`build-info.json` 和 `SHA256SUMS.txt` 为准。不要仅为更换页脚文字而把不同字节的固件当作同一验收产物。
