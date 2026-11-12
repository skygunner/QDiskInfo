# QDiskInfo GUI 重写选型与设计方案

状态：原方案于 2026-09-23 定稿；Magician、Crystal 和 Classic 视图已实现。2026-11-12 补充当前扫描实现与验证状态。
范围：原方案仅涉及视图层，计划保留 `utils`、`jsonparser`、`asciiview` 和健康度判定逻辑；此后 `utils` 已为异步扫描与 USB 桥兼容性修改。

第 1–5 节保留的是**当时的诊断、设计设想和验收目标**，其中的布局、文件名、架构示意和验收条件不代表逐项实现或通过。当前实现和待验证事项见第 6–7 节。

---

## 1. 现状诊断

| 项 | 现状 | 问题 |
|---|---|---|
| 框架 | Qt6 Widgets（`QMainWindow` + `.ui` 文件），C++17 | 框架本身没问题，问题在视图层 |
| 视觉 | 系统 QPalette 原生样式 + 少量内联 `setStyleSheet`（如纯绿 `rgb(0,255,0)` 大色块） | 纯绿/纯黄/纯红背景块刺眼；`QLineEdit` 展示只读数据显得像表单；无排版层级 |
| 布局 | 顶部盘符按钮条（`CustomButton` 自绘三行文本）+ 中部左右两组 `QFormLayout` + 底部 SMART 表 | 信息层级弱，间距松散，"GNOME 泡泡感"来自原生控件默认样式与零主题定制 |
| 主题能力 | 无。仅 `dist/theme` 里的可选动漫皮肤（编译期开关） | 无统一设计 token，颜色硬编码 |

结论：**不是框架问题，是"从未做过视觉设计"的问题**。数据层（scan/parse/健康度/TBW 换算）质量不错且经过实战验证，应原样保留。

## 2. Linux 上可选的 GUI 技术方案对比

| 方案 | 语言/运行时 | 视觉上限 | 复用现有 C++ 数据层 | 打包/体积 | 风险 | 结论 |
|---|---|---|---|---|---|---|
| **A. Qt Widgets + 全局 QSS + QPainter 自绘控件** | C++（现有） | 高——QSS 全控颜色/边框/圆角/间距，QPainter 可画任意仪表盘/环形进度 | **100% 直接复用** | 不变（单二进制，现有 CI/AppImage 直接可用） | 低 | ✅ **采用** |
| B. Qt Quick (QML) 嵌入 | C++ + QML | 很高（声明式+动画+着色器） | 需暴露 Q_OBJECT 模型层 | +Qt6 Quick 运行时依赖 | 中：QML 桌面表格/菜单/托盘集成弱，重写量 ≈ 全部 UI 代码，且引入第二语言 | 否 |
| C. GTK4 + libadwaita | C/Rust/Vala（gtkmm 一般） | 中——且 libadwaita 正是用户点名讨厌的"软泡泡" GNOME 风 | 0%（断裂） | 新依赖栈 | 高 | 否 |
| D. Electron | JS/TS | 高（CSS 随便画） | 0%（smartctl 走 node 子进程，逻辑重写） | 150MB+ 运行时 | 内存/体积差，违背本工具轻量定位 | 否 |
| E. Tauri | Rust + Web 前端 | 高 | 0%（Rust 重写数据层） | 较小但工具链断裂 | 中高 | 否 |
| F. Flutter Linux | Dart | 高（自绘 Skia） | 0% | +40MB 自绘运行时，桌面集成（菜单/文件对话框/系统主题）弱 | 高 | 否 |
| G. Slint / Avalonia / wxWidgets | Rust / C# / C++ | 中 | 0% 或弱绑定 | 各有额外栈 | 中~高 | 否 |

**选型理由（方案 A）**：
1. 视觉瓶颈完全可以在 Qt Widgets 内解决：QSS 对 QWidget 的控制力等价于 CSS 对 HTML（含伪状态 `:hover/:checked`、盒模型、`qlineargradient`）；环形仪表、温度弧这类"CSS 画不了"的控件用 `QPainter` 自绘，工作量可控且渲染质量高（反锯齿、HiDPI 免费获得）。
2. 数据层零改动 = 零回归风险。smartctl 的 pkexec 提权链路、ATA/NVMe/SCSI 三协议解析、健康度判定规则是本项目最有价值的资产。
3. 打包、CI（`.github/workflows`）、AppImage 脚本、翻译（`lupdate` 扫描源码）全部继续工作。
4. 两套目标风格（Magician 深色卡片 / CDI 硬朗密表）都是"桌面工具风"，Widgets 是这类 UI 的舒适区。

**架构：单数据层 + 双皮肤（可切换）**
```
utils / jsonparser / asciiview          ← 不动
MainWindow(数据编排、菜单、动作)          ← 保留类名与全部槽函数
 ├── UiMode::Magician  → MagicianView   ← 方案1：深色侧栏 + 卡片仪表盘
 ├── UiMode::Crystal   → CrystalView    ← 方案2：硬朗高密度工具风
 └── theme::Manager                      ← QSS 资源 + 设计 token + 调色板注入
ThemeSwitch: 菜单 View → Interface → Magician / Crystal，存 QSettings
```

---

## 3. 方案1：Samsung Magician 风格（深色仪表盘）

### 3.1 设计语言（从 Example/Samsung 提取 + Magician 8.x 实机布局）

调色板实测（示例截图主色提取）：背景近黑 `#181717/#1D1C1D`，卡片抬升面 `#242323/#2A2929`，点缀 `#364351`（蓝灰）、`#357785`（青）。这确认了 Magician 的视觉基调：**近黑底、同色系灰阶分层、单一冷色点缀、大数字排版**。

设计 token（`MagicianDark`）：

| Token | 值 | 用途 |
|---|---|---|
| `bg/base` | `#141517` | 窗体底 |
| `bg/sidebar` | `#0E0F11` | 侧栏（比底更深） |
| `bg/card` | `#1E2023` | 卡片面 |
| `bg/cardHover` | `#26282C` | 卡片悬停 |
| `bg/field` | `#17181B` | 值域底 |
| `stroke` | `#2C2F34` | 1px 卡片描边 |
| `text/primary` | `#E8EAED` | 主文本 |
| `text/secondary` | `#9AA0A6` | 次文本 |
| `accent` | `#29B6C8`（青） | 选中态、进度环、焦点 |
| `accent/press` | `#1E8A99` | 按压 |
| `ok` / `warn` / `bad` / `na` | `#37C871` / `#F5B301` / `#F0475A` / `#8A9199` | 健康四态（低饱和，不刺眼） |

### 3.2 布局（复刻 Magician 三段式）

```
┌──────┬────────────────────────────────────────────────┐
│ █ logo │  DRIVE DETAILS      (页面标题)    [⟳ Refresh] │  顶栏 h=48
│      ├────────────────────────────────────────────────┤
│  🖴   │  ┌──────────────────────────────────────────┐  │
│  📊   │  │ ============ Hero 卡 ==================  │  │
│  🧪   │  │  ┌─────┐  SAMSUNG SSD 990 PRO 2TB       │  │
│  ⚙    │  │  │ 98% │  GOOD · NVMe · /dev/nvme0      │  │
│      │  │  │ (环) │  Serial ▸ FW ▸ Interface ▸ ...  │  │
│ 侧栏  │  │  └─────┘  TBW / 温度 / 通电 三块迷你指标  │  │
│ w=64 │  └──────────────────────────────────────────┘  │
│ 纯icon│  ┌─ 盘选择条：横向卡片（每盘一卡，环+型号）──┐  │
│      │  │ [◉ nvme0 990PRO] [○ sda WD Blue] …       │  │
│      │  └──────────────────────────────────────────┘  │
│      │  SMART 属性表（深色表格·斑马纹·状态点）        │
│      │  Tabs: [Attributes] [Self-Test Log] [ASCII]    │
└──────┴────────────────────────────────────────────────┘
```

要点：
- **左侧纯图标窄侧栏**（64px，icon-only，悬停出 tooltip），当前只有一个有效页（Drive Details），其余为占位禁用态——Magician 本身就是多页但本工具只有详情页，侧栏保留是为构图完整。
- **Hero 卡**：左侧 `HealthRing`（QPainter 自绘环形仪表，240° 弧，中心大号百分比 + 状态词），右侧型号大标题 + `Serial/Firmware/Interface/Protocol/Device node/Capacity` 两列键值网格；卡底一行三枚迷你指标（Total Host Reads / Total Host Writes / Power On Hours）。
- **温度**：独立小环或数字徽章，颜色按现有阈值逻辑（warn/critical）。
- **表格**：QSS 定制 `QTableWidget`——无外框、表头底 `#17181B` 大写小字号、行高 32、斑马纹 `#1B1D20`、右对齐等宽数字。状态列用现有 `StatusDot` 委托（换 token 色）。
- **字体**：默认 sans（遵循系统），数值标签 `QFont::setStyleHint(Monospace)`+`setFontLetterSpacing` 由 QSS `font-family` 统一；标题字重 600/700。
- 全部直角**或 6px 微圆角**（Magician 卡片约 8px，取 6px 更硬朗一点），无阴影渐变，靠描边+明度分层——这是"商务技术风"的关键。

### 3.3 实施清单（方案1）

1. `src/theme/manager.{h,cpp}`：QSS 按 `UiMode` 应用到 `qApp`；暴露 token（`Theme::color(Role)`），供自绘控件取色，保证换肤一致。
2. `src/theme/magician.qss`（qrc 嵌入）：覆盖 QMainWindow/QMenu/QMenuBar/QPushButton/QLineEdit/QTableWidget/QScrollBar/QToolTip/QMessageBox/QDialog。
3. `src/widgets/healthring.{h,cpp}`：环形仪表（值 0-100、状态色、中心两行文本、HiDPI）。
4. `src/widgets/sidebar.{h,cpp}`：icon-only 导航。
5. `src/widgets/drivecard.{h,cpp}`：盘选择卡（checkable，选中态描边+accent）。
6. `src/magicianview.{h,cpp}`：组装上述部件；暴露与旧布局等价的数据接口（由 MainWindow 喂数据）。
7. `MainWindow`：加 `UiMode` 状态与切换菜单；`populateWindow` 双路由（按模式喂旧控件组或新视图）；菜单栏保留（QSS 深色化）。
8. 构建验证 + offscreen 截图评审。

## 4. 方案2：CrystalDiskInfo 现代硬朗风（Crystal）

### 4.1 设计语言

CDI 实测调色板：标题深蓝灰 `#253D58`、冰蓝 `#B8E2EC/#E2F9FD`、正文灰白。方向：**保留 CDI 的信息架构（一屏看全所有盘 + 当前盘细节 + SMART 密表），去掉 Win98 感，做浅色商务技术风**；不抄 GNOME（无大圆角、无气泡按钮、无 libadwaita 头部）。

设计 token（`CrystalLight`，另备 `CrystalDark` 同构暗色）：

| Token | 值 | 用途 |
|---|---|---|
| `bg/base` | `#F4F6F8` | 窗体底（冷灰白） |
| `bg/panel` | `#FFFFFF` | 面板 |
| `bg/header` | `#22364E` | 区块标题条（CDI 深蓝灰的现代版） |
| `stroke` | `#D4DAE0` | 1px 分隔/描边 |
| `stroke/strong` | `#9AA6B2` | 表格外框 |
| `text/primary` | `#1A2530` | 正文 |
| `text/secondary` | `#5A6B7B` | 标签 |
| `accent` | `#0B66C3` | 选中/链接/焦点 |
| `ok/warn/bad/na` | `#1E9E4F / #C77700 / #D5383E / #8A9199` | 四态（比方案1 更沉稳） |

硬朗 = **2px 圆角或直角、1px 实线边框、无渐变、无阴影、紧凑行高（表格 24-26px）、大写小字号表头、tabular 数字**。

### 4.2 布局

```
┌────────────────────────────────────────────────────────────┐
│ 设备总览表（每盘一行：状态点│型号│容量│温度│健康│协议│节点）│ ← 紧凑表格，行即选择器
├───────────────┬────────────────────────────────────────────┤
│ 盘徽章块       │  IDENTIFICATION          STATISTICS        │
│ [环:健康%]     │  Model      990 PRO      Total Reads  336GB│
│ 温度 41°C     │  Firmware   4B2QGXA7     Total Writes 1.5TB │
│ Good          │  Serial     S6Z1NJ…      Power On Hrs 1234  │
│               │  Protocol   NVMe         Power Cycles 567   │
│ (深蓝灰底块)   │  ────────────────────────────────────────── │
├───────────────┴────────────────────────────────────────────┤
│ SMART ATTRIBUTES （ID│名称│当前│最差│阈值│Raw，密行，细网格）│
└────────────────────────────────────────────────────────────┘
```

要点：
- 顶部设备**总览表取代泡泡按钮条**——CDI 精髓（一屏看全所有盘），同时是选择器：点击行切换。行内状态点复用健康判定色。
- 左侧盘徽章块：小号 `HealthRing`（复用方案1 的自绘控件，浅色 token）+ 温度徽章 + 健康词，底色 `bg/header` 深蓝灰块，白字——这就是 CDI 左栏的"现代版"。
- 右侧双栏：IDENTIFICATION（只读，`QLineEdit` 换成无边框值控件）/ STATISTICS。
- SMART 表：细网格（`gridline-color`）、行高 24、表头白字深底、右对齐数字、状态点列保留。
- 自检入口仍走 Device 菜单（菜单 QSS 浅色硬朗化）。

### 4.3 实施清单（方案2）

1. `src/theme/crystal.qss`（+ 可选 crystal_dark.qss）。
2. `src/crystalview.{h,cpp}`：设备总览表 + 徽章块 + 双栏信息 + SMART 表组装。
3. 复用：`HealthRing`（参数化配色）、`StatusDot`、`MainWindow` 数据路由、全部动作/菜单/设置。
4. 切换：菜单 View → Interface → Magician / Crystal（QSettings 持久化，即时重建 centralwidget）。

## 5. 里程碑与验收

| 阶段 | 交付 | 验收 |
|---|---|---|
| M1 | 方案1 Magician 深色仪表盘 | 编译零警告；offscreen 截图对照 Example/Samsung 构图；三协议盘（ATA/NVMe/SCSI）数据正确渲染；菜单/对话框深色化 |
| M2 | 方案2 Crystal 硬朗风 | 同上，对照 CDI 信息架构；浅色为主、同构暗色可选；两种模式即时切换并持久化 |
| 回归 | `--ascii-view`、自检流程、JSON/图像导出、翻译扫描 | 行为与重写前一致 |

风险与对策：
- **QSS 对 QTableWidget 表头/网格控制有限** → 用 `setStyleSheet` 逐属性 + `QHeaderView` API 双管齐下；实在不够的地方用 ItemDelegate 画。
- **HiDPI 下 QPainter 自绘模糊** → 全部按 `devicePixelRatio` 缩放，禁止整数像素假设。
- **旧 `.ui` 布局被新视图替代后的回退** → 旧代码路径保留（Classic 模式可作为第三选项），新视图纯增量代码。

---

## 6. 实施记录（2026-09-23）

Magician 和 Crystal 视图及 Classic 回退已实现；当时记录曾进行离屏截图检查（`shots/`，不入库）。这不等于第 5 节全部验收项均已验证。文件清单：

| 文件 | 作用 |
|---|---|
| `include/theme.h` / `src/theme.cpp` | 设计 token 单一来源；`Theme::apply()` 装 Fusion 基座 + 调色板 + QSS 模板替换；`Theme::statusColor(mode, state)` 供视图渲染期解析状态色 |
| `src/theme/magician.qss` / `crystal.qss` | 两套 QSS 模板（`@token@` 占位符） |
| `src/theme/icons/*.svg` | 侧栏内嵌图标（不依赖桌面主题） |
| `include/healthring.h` / `src/healthring.cpp` | QPainter 环形仪表（270° 弧、圆头、中心双行文本、HiDPI） |
| `include/sidebar.h` / `src/sidebar.cpp` | Magician 式 icon-only 导航栏（WA_StyledBackground） |
| `include/drivecard.h` / `src/drivecard.cpp` | 盘选择卡片（选中态 accent 描边） |
| `include/tableutil.h` / `src/tableutil.cpp` | 两视图共用的表格填充（状态点列/对齐/伸缩列） |
| `include/magicianview.h` / `src/magicianview.cpp` | 方案1 页面：侧栏 + DRIVE DETAILS 头栏 + Hero 卡（健康环/字段网格/温度环/四枚指标块）+ 盘卡条 + SMART 表 |
| `include/crystalview.h` / `src/crystalview.cpp` | 方案2 页面：设备总览表（行即选择器）+ 深蓝灰徽章块 + IDENTIFICATION/STATISTICS 双栏 + SMART 密表 |
| `include/disksummary.h` | 视图数据契约（`DiskSummary`/`DriveSlot`/`SummaryTable`，颜色以 state 字符串传递，渲染期映射 token，避免换肤后残留旧色） |

### 集成方式
- `MainWindow` 保留全部槽/菜单/数据管线；构造时把 `.ui` 的 gridLayout 包装为 `classicPage`，与两个新视图同层切换可见性。
- 菜单新增 **View → Magician / Crystal / Classic Style**（exclusive），选择持久化在 `QSettings ui/mode`，默认 Magician。
- `populateWindow()` 在原有计算路径上顺带填充 `summary`，末尾按当前模式分发给视图；`updateUI()` 同步 `driveSlots` 到盘卡条/设备表。
- 盘切换仍走隐藏的 CustomButton `click()`（复用既有独占组与导航逻辑），键盘 Forward/Back、鼠标侧键照旧。

### 开发/验证钩子（保留为常设工具）
- `QDISKINFO_DEMO=1`：`utils::scanDevices` 返回 qrc 内嵌样例（NVMe + ATA），无 root/无盘也能起界面。
- `QDISKINFO_SCREENSHOT=path`：启动 1.2s 后 `grab()` 存图并退出，配合 `QT_QPA_PLATFORM=offscreen` 可脚本化截屏（本次验证即用此法）。
- `QDISKINFO_SWITCH_TO=magician|crystal|classic`：截图前 0.5s 运行时切换一次界面模式，专门用于回归 classic↔themed 切换场景。

### 过程中修掉的问题
1. **崩溃**：原构造函数直接 `delete` 仍在 .ui formLayout 里的 4 个 horizontal 标签；布局被重父化后留下悬空 layout item，`grab()`/布局计算触发 SIGSEGV。改为 `hide()`（功能等价，见 mainwindow.cpp 注释）。
2. **Qt `slots` 宏**：参数名 `slots` 会宏展开破坏代码，已更名 `driveList`。
3. **QWidget 子类 QSS 背景不渲染**：Sidebar 需 `WA_StyledBackground`。
4. **Classic 残留在新视图背后**：`buildUiModePages()` 最初用 `addItem()` 把 .ui 的 gridLayout 挂进 classicPage——`addItem()` 不会 re-parent 子布局里的控件（那是 `addLayout()` 的行为），控件仍是 centralwidget 的子节点，藏页面时它们原样留在屏幕上。改用 `takeAt(0)` + `addLayout(gridLayout)` 后，classic 全部控件随页面隐藏/显示。已用 `QDISKINFO_SWITCH_TO` 离屏回归（classic 启动→运行时切换，扫描截图确认无 classic 纯绿/黄/红残留；反向 themed→classic 页面完整渲染）。
5. **纯色状态色**：旧 `Qt::green/yellow/red` 曾流入新视图；改为 state 字符串 + 渲染期 token 映射。

## 7. 当前实现与验证状态（2026-11-12）

### 需求与实现

- 界面启动不应等待慢盘或权限请求：窗口启动后通过 `QProcess` 异步运行一个 `pkexec` 扫描助手，先返回设备列表，再顺序读取并逐盘更新界面。扫描期间禁止重复刷新；普通单盘超时或失败后继续处理其他盘。每次 smartctl 读取的超时为 20 秒；若进程卡在无法及时终止的内核 I/O 中，助手会停止后续读取。自检命令仍同步等待。
- 对自动识别为 `sntjmicron` 的设备，先尝试 `-a --json=o -d sat,12`。仅当命令无相关错误、协议为 ATA 且包含有效 SMART 属性时采用该类型；否则以检测到的类型和 `-x` 回退。成功类型会用于后续自检。其他设备仍用检测类型的 `-x`。
- 三种风格通过 **View → Magician / Crystal / Classic Style** 切换，`ui/mode` 持久化，默认 Magician；页面通过显示/隐藏切换。原方案中的具体侧栏、布局细节、文件路径和可选暗色主题是设计设想，不应视作逐项交付。

### 已验证与待验证

- 本次使用现有 `build` 目录执行 `cmake --build build -j 2` 和 `ctest --test-dir build --output-on-failure`，构建成功，`scan_tests` 通过。测试使用模拟 `smartctl` / `pkexec` 和 Qt offscreen，覆盖渐进扫描、重复刷新、超时、权限失败、`sat,12` 接受及回退等路径，不代表真实硬件验证。
- 用户报告其 USB 转接 mSATA 盘上 `sudo timeout 15s smartctl -a -d sat,12 /dev/sdc` 成功，`-d sntjmicron` 和 `-d sat` 超时。这是独立命令的结果；**QDiskInfo 在该真实 USB 设备上的完整扫描、显示和后续操作仍待验证**。
- 第 5 节的 ATA/NVMe/SCSI 实盘渲染、菜单及对话框、导出、自检、翻译等完整回归仍需逐项验证；历史离屏截图不能替代这些验收。
