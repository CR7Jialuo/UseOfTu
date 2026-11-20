# 通宵进度存档（上下文压缩后从这里恢复）

> 目标：把校园导航系统做成可交付的完整应用。
> 每完成一个里程碑：更新本文件 → `git add -A && git commit && git push`。

## 一、当前状态（滚动更新）

| 项 | 状态 |
|---|---|
| 核心库（数据/算法/CSV/快照） | ✅ 完成，1592 项断言通过 |
| 真实路网（353 节点 / 409 边 / 22.4 km） | ✅ 完成 |
| 命令行版 `build\nav.exe` | ✅ 全部子命令实测通过 |
| 图形版 `build\navgui.exe` | ✅ 可运行；待办：DPI 缩放、图标、细节 |
| 报告（学校模板，26 页） | ✅ 已生成；封面 4 项待用户补 |
| PPT（12 页） | ⚠️ 旧版，需按真实数据/GUI 更新 |
| 提交包 `dist\campus-nav-submit.zip` | ✅ 含双 exe；需随文档更新重打 |
| GitHub `CR7Jialuo/UseOfTu` | ✅ 已推送；CI 绿 |

## 二、关键坐标（改代码时用）

```
仓库根：C:\Users\ZhuanZ\Downloads\learn\campus-nav
工具链：.tools\w64devkit\w64devkit\bin\gcc.exe（GCC 16.2，x64）
Python：C:\Users\ZhuanZ\.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\python\python.exe
LibreOffice Kit：D:\program\deepseek\resources\app.asar.unpacked\dsh\node_modules\@deepseek-ai\libreoffice-kit\lib\cli.js
模板：build\template\template.docx（8 分节；封面表格 5 行）
```

## 三、必须记住的坑

1. **GDI+ 在 C 里**：用 flat API（`GpBitmap`/`GpGraphics`/`GpSolidFill`），`GdipCreateSolidFill` 不是 `GdipCreateSolidBrush`；
   `GdipDrawString(g, text, -1, font, &rect, NULL, brush)` —— StringFormat 在 brush 之前。
2. **中文**：源码 UTF-8，运行时 `MultiByteToWideChar`；不要用 `L"\6559"`（那是八进制不是 Unicode 转义）。
3. **GUI 子进程**会让命令挂住 → 用 `--selftest`（开窗 800ms 自动关）验证窗口模式。
4. **GCC 临时目录**：脚本里把 TMP/TEMP 指向 `build\tmp`，否则报 "Cannot create temporary file"。
5. **make_thesis.py 的 Writer**：python-docx 的新段落插在末尾 sectPr 之前，按长度切片会漏；
   按 `id()` 去重会因 lxml 元素释放后 id 复用而错乱 → 用**临时属性标记**做 diff。
6. **报告高亮**：模板封面带 `w:highlight` 与单元格 `w:shd`，要一并清除。

## 四、构建与验证命令

```powershell
# 命令行版
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\test.ps1      # 1592 断言
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\e2e.ps1       # 14 场景

# 图形版
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build_gui.ps1
.\build\navgui.exe --selftest
.\build\navgui.exe --shot out.png --route 1 5 --size 1440x920

# 报告（两遍法：先出 PDF 定位标题，再回填目录页码）
python docs\tools\find_pages.py build\thesis_pass1.pdf docs\toc_entries.json docs\thesis_pages.json
python docs\tools\make_thesis.py --pages docs\thesis_pages.json --out docs\课程设计报告.docx

# 打包
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\export_submission.ps1
```

## 五、待办队列（从上下往下做）

- [x] M-A GUI：`.ico` 图标（windres 链入）、高 DPI 下窗口尺寸放大、导出当前视图菜单、路径逐段动画
- [x] M-B GUI：视图菜单(缩放/关键道路)、快捷键说明、方向键平移、--open 定位
- [x] M-C 应用：`--open` 定位、e2e 增加 GUI 离屏渲染与自检两项检查
- [x] M-D 文档：PPT 改版（真实数据 + GUI 截图，12 页）、报告插图换真实路网（27 页）
- [x] M-E 交付：重打 zip（112 文件、6.07 MB、含双 exe）、README 附图、CI 复验

## 六、已知环境问题（不是 bug）

- 页眉/页脚的“共 N 页”是 NUMPAGES 域：LibreOffice 转换 PDF 时会按它自己的算法算（比实际多 1~2 页），
  原模板转换也一样（13 页却显示“共 14 页”）。Word 打开会重新计算，无需处理。

## 七、最近一次提交

见 `git log -1`。

## 八、已验证的关键事实（换环境时先查这里）

- 从 GitHub 全新 clone 后：`scripts\build.ps1` 与 `scripts\build_gui.ps1` 均能出 exe（需 gcc 在 PATH 或设 `CC`）
- `nav.exe --data data/campus_real check`：353 地点 / 409 道路 / 连通 / 141 割点 / 190 桥
- `navgui.exe --selftest`：开窗 800ms 自动关闭，退出码 0
- `navgui.exe --shot x.png --route 1 5`：渲染完整路径（动画在截图模式自动关闭）
- `scripts\e2e.ps1` 末尾含两项 GUI 检查（离屏出图 >20KB、自检退出码 0）
- Windows CI 作业：choco 装 MinGW → 构建 CLI+GUI → 跑测试 → 离屏渲染 → 上传截图
