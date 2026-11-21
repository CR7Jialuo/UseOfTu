# 开发进度存档（上下文压缩后从这里恢复）

> 目标：把校园导航系统做成可交付的完整应用。
> 每完成一个里程碑：更新本文件 → `git add -A && git commit && git push`。

## 一、完成情况

| 项 | 状态 |
|---|---|
| 核心库（数据 / 算法 / CSV / 快照） | ✅ 22 组 1592 项断言全通过 |
| 真实路网（353 节点 / 409 边 / 22.4 km） | ✅ 完成并有回归测试 |
| 命令行 `build\nav.exe` | ✅ 全部子命令实测通过 |
| 图形 `build\navgui.exe` | ✅ 底图 / 路网 / 路径动画 / 巡游 / 关键道路 / 导出 / DPI / 图标 |
| 报告（学校模板） | ✅ 28 页，含真实路网与多点巡游插图 |
| PPT | ✅ 12 页 + 每页演讲者备注 |
| 提交包 | ✅ `dist\campus-nav-submit.zip`（108 文件，双 exe + 文档） |
| CI | ✅ Linux 与 Windows 两个作业，Windows 会构建 GUI 并离屏出图 |
| 使用说明 | ✅ `docs/使用说明.md` |

## 一之一、代码审查与清理（2026-11-21）

**修掉的问题**

1. 底图世界范围写死 `2572×2582` → 改按实际图片尺寸，换底图不会错位
2. 巡游代价矩阵可能读未初始化内存 → 取值加边界检查，矩阵改用 `calloc`
3. `ui.c` 地名拼接缓冲 64 字节偏小 → 160（`-fanalyzer` 报截断）
4. `gui_init` 路径缓冲 512 与 `data_dir` 拼接可能截断 → 1024
5. 窗口可缩到 0 宽导致布局错乱 → `WM_GETMINMAXINFO` 限制最小 900×600
6. 拖到另一块不同 DPI 的显示器后不重排 → 处理 `WM_DPICHANGED`
7. 画笔/画刷缓存未释放 → `gui_render_shutdown` 统一释放

**清理**

- `build/`：594 文件 / 65.4 MB → 2 文件（仅两个 exe） / 0.5 MB
- 删除旧报告生成器 `docs/tools/make_report.py` + `docs/toc_pages.json`
- 删除无人引用的图：`route_gate_library` / `route_gym_dorm` / `routing-demo` / `test-summary`
- `scripts/make_docs.ps1` 重写为论文模板流水线（7 步，一条命令出全部文档）
- `scripts/export_submission.ps1` 打包后自动清理 `dist/stage`

**已知非 bug**：页眉“共 N 页”是 NUMPAGES 域，LibreOffice 转换会比实际多 1~2 页；未签名 exe 首次运行会触发 SmartScreen。

## 二、关键坐标

```
仓库根：C:\Users\ZhuanZ\Downloads\learn\campus-nav
工具链：.tools\w64devkit\w64devkit\bin\gcc.exe（GCC 16.2，x64；windres 同目录）
Python：C:\Users\ZhuanZ\.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\python\python.exe
LibreOffice Kit：D:\program\deepseek\resources\app.asar.unpacked\dsh\node_modules\@deepseek-ai\libreoffice-kit\lib\cli.js
模板：docs\template\template.docx（8 分节；封面表格 5 行）
```

## 三、必须记住的坑

1. **GDI+ 在 C 里**：flat API（`GpBitmap`/`GpGraphics`/`GpSolidFill`），`GdipCreateSolidFill` 不是
   `GdipCreateSolidBrush`；`GdipDrawString(g, text, -1, font, &rect, NULL, brush)` —— StringFormat 在 brush 之前。
2. **中文**：源码 UTF-8，运行时 `MultiByteToWideChar`；`L"\6559"` 是八进制不是 Unicode 转义。
3. **GUI 子进程**会让命令挂住 → 用 `--selftest`（开窗 800ms 自动关）验证窗口模式。
4. **GCC 临时目录**：脚本把 TMP/TEMP 指向 `build\tmp`，否则报 "Cannot create temporary file"。
5. **make_thesis.py 的 Writer**：python-docx 新段落插在末尾 sectPr 之前，按长度切片会漏；
   按 `id()` 去重会因 lxml 元素释放后 id 复用而错乱 → 用**临时属性标记**做 diff。
6. **报告高亮**：模板封面带 `w:highlight` 与单元格 `w:shd`，要一并清除。
7. **DPI**：面板坐标写成 96 DPI 逻辑值，绘制时乘 `s_dpi/96`（`UI()` 宏）；字体在 DPI 变化时重建。

## 四、构建与验证命令

```powershell
# 命令行版
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\test.ps1      # 1592 断言
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\e2e.ps1       # 14 场景 + 2 项 GUI

# 图形版（含 windres 资源）
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build_gui.ps1
.\build\navgui.exe --selftest
.\build\navgui.exe --shot out.png --route 1 5 --size 1440x920
.\build\navgui.exe --shot out.png --route 1 5 --via 6,7,11 --dpi 144

# 报告（两遍法：先出 PDF 定位标题，再回填目录页码）
python docs\tools\make_thesis.py --out build\pass1.docx
#   LibreOffice 转 pass1.docx -> pass1.pdf
python docs\tools\find_pages.py build\pass1.pdf docs\toc_entries.json docs\thesis_pages.json
python docs\tools\make_thesis.py --pages docs\thesis_pages.json --out docs\课程设计报告.docx

# 打包
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\export_submission.ps1
```

## 五、已完成里程碑

- [x] M-A GUI：`.ico` 图标（windres 链入）、高 DPI 下窗口尺寸放大、导出当前视图、路径逐段动画
- [x] M-B GUI：视图菜单（缩放 / 关键道路）、快捷键说明、方向键平移、`--open` 定位
- [x] M-C 应用：多点巡游（右击加途经点、编号标记、`T` 规划）、e2e 增加 GUI 检查
- [x] M-D 文档：PPT 改版（真实数据 + GUI 截图 + 演讲备注）、报告换真实路网与巡游插图
- [x] M-E 交付：重打 zip（含双 exe）、README 重写、使用说明
- [x] M-F CI：Windows 作业全绿（MinGW → 构建 CLI+GUI → 测试 → 离屏渲染 → 上传截图）
- [x] M-G GUI：界面整体 DPI 缩放（面板 / 字体 / 状态栏 / 提示卡按 96→实际 DPI 等比放大）

## 六、已知环境问题（不是 bug）

- 页眉“共 N 页”是 NUMPAGES 域：LibreOffice 转 PDF 时按自己的算法算（比实际多 1~2 页），
  原模板转换也一样（13 页却写“共 14 页”）。Word 打开会重算。

## 七、待用户补的信息

**封面四项**：学院 / 专业班级 / 指导教师职称 / 企业导师。
填 `docs/tools/make_thesis.py` 顶部 `PERSON` 字典后重跑两遍法即可。

## 八、已验证的关键事实（换环境时先查这里）

- 从 GitHub 全新 clone 后：`scripts\build.ps1` 与 `scripts\build_gui.ps1` 均能出 exe（需 gcc 在 PATH 或设 `CC`）
- `nav.exe --data data/campus_real check`：353 地点 / 409 道路 / 连通 / 141 割点 / 190 桥
- `nav.exe --data data/campus_real route 1 5`：1114.6 米 / 22.3 分钟
- `navgui.exe --selftest`：开窗 800ms 自动关闭，退出码 0
- `navgui.exe --shot x.png --route 1 5`：渲染完整路径（动画在截图模式自动关闭）
- `scripts\e2e.ps1` 末尾含两项 GUI 检查（离屏出图 > 20KB、自检退出码 0）
- Windows CI 作业：choco 装 MinGW → 构建 CLI+GUI → 跑测试 → 离屏渲染 → 上传截图
