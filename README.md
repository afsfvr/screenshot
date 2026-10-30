# Screenshot - 跨平台截图工具

基于 Qt 开发的跨平台截图工具，支持 Windows 和 Linux(X11)，集截图、贴图、GIF 录制、长截图、OCR、二维码识别与生成等功能于一体。

## 功能特性

- **跨平台**：支持 Windows 和 Linux（仅 X11/Xorg 会话）
- **截图模式**：支持矩形选区、自由拖动选区、自动识别窗口选区、全屏截图；支持多屏幕拼接截图，各屏幕独立 DPI 缩放
- **GIF 录制**：区域录制 GIF（快捷键启动），帧率 1~30 可调，录制帧数过大时自动落盘缓冲
- **长截图**：截取可见区域后滚动鼠标滚轮，基于 OpenCV 模板匹配自动拼接长图
- **贴图置顶**：截图可置顶悬浮于桌面（图钉），支持调整大小、透明度、锁定位置、上下滚动查看长图
- **图片编辑**：矩形、椭圆、直线、画笔、箭头、文字等绘制工具，支持填充、颜色、透明度、描边样式，可撤销/恢复编辑
- **保存与复制**：保存到文件或剪贴板，支持 QImageWriter 全部格式（默认 png）；截图时按快捷键可直接保存到指定目录
- **取色**：截图时显示放大镜并实时预览鼠标处像素 RGB，按 `C` 复制到剪贴板
- **OCR**：多后端支持——RapidOCR（本地 Onnx）、PaddleOCR（本地 PP-OCRv5）、腾讯云 OCR 可同时编译，在设置界面下拉切换；识别结果可框选、编辑、复制
- **二维码**：识别图中二维码/条形码，或将输入文字生成为二维码
- **其他**：托盘常驻、自定义全局热键、开机自启动（用户/全用户）、单实例运行、DPI 缩放适配

## 构建依赖

- CMake >= 3.16
- Qt，优先 Qt6，自动回退 Qt5（组件：Core、Gui、Widgets；启用腾讯云 OCR 需要 Network；Linux 下 Qt5 需要 X11Extras）
- C++20 编译器（MSVC / GCC / Clang）
- 可选依赖：
  - OpenCV（core, imgproc, imgcodecs）：未安装时仅禁用长截图功能，其余功能不受影响
  - ZXing（二维码，默认启用，CMake FetchContent 自动下载，无需手动安装）
  - GIF 编码器内置于 `third_party/gif`，无需安装

### Linux（Ubuntu/Debian）

```bash
sudo apt install build-essential cmake qtbase5-dev libqt5x11extras5-dev \
    libx11-dev libxtst-dev libxrandr-dev libopencv-dev
```

### Windows

安装 [Qt](https://www.qt.io/)（含 CMake）与 Visual Studio 2022，使用 `x64 Native Tools Command Prompt` 执行下述命令。

## 编译

```bash
cmake -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### CMake 选项

| 选项 | 默认 | 说明 |
|---|---|---|
| `TENCENT_OCR` | OFF | 启用腾讯云 OCR 后端（需配网，密钥在设置中加密保存） |
| `RAPID_OCR` | OFF | 启用 RapidOCR 本地推理，需自行编译 [RapidOcr](https://github.com/RapidAI/RapidOCR)，并用 `RapidOcrConfig.cmake` 指向其安装目录（onnx 模型位于 `third_party/rapidocr/models`） |
| `PADDLE_OCR` | OFF | 启用 PaddleOCR 本地推理，需通过 `-DPADDLE_LIB=<路径>` 指定 [PaddleOCR](https://github.com/PaddlePaddle/PaddleOCR) 预编译库 |
| `ENABLE_ZXING` | ON | 启用二维码识别与生成 |
| `STATIC_LINK` | OFF | 静态链接运行时库（`-static`，便于分发） |

三个 OCR 选项相互独立、可任意组合同时启用；多个后端编译进同一程序后，在设置界面通过下拉框切换使用哪一个，各后端的参数独立保存。全部关闭时 OCR 功能不可用。示例：

```bash
# 同时启用腾讯云与 RapidOCR（需要准备 RapidOcr 库）
cmake -Bbuild -DCMAKE_BUILD_TYPE=Release -DTENCENT_OCR=ON -DRAPID_OCR=ON
```

### 打包

```bash
cmake --build build --target package
```

Linux 自动生成 DEB 包；Windows 生成 NSIS 安装包。

## 使用说明

- 程序启动后最小化到托盘，右键托盘图标可打开设置、截图、录制 GIF
- 可在设置中为「截图 / 录制 GIF / 自动保存」配置全局热键（Ctrl/Alt/Shift + A~Z 任意组合）

### 快捷键

| 按键 | 功能 |
|---|---|
| 全局热键（自定义） | 启动截图 / 录制 GIF / 直接截图并自动保存 |
| `Esc` | 取消选区 / 关闭截图窗口 |
| `Ctrl+C` | 复制图片到剪贴板 |
| `Ctrl+S` | 保存图片到文件 |
| `Ctrl+Z` / `Ctrl+Y` | 撤销 / 恢复编辑 |
| `C` | 复制鼠标处像素 RGB |
| `←` `↑` `→` `↓` | 微调选区 / 置顶窗口位置 |
| 滚轮 或 `Ctrl+滚轮` | 长截图滚动 / 缩放置顶窗口（可在设置中切换） |
| 右键 | 取消选区，或在置顶窗口中打开功能菜单 |

### 配置文件

配置保存在 `~/.config/screenshot/screenshot.data`（优先使用可执行文件同目录）。卸载时自动清理自启动项。

## 仓库

- 主仓库：<https://github.com/afsfvr/screenshot>
