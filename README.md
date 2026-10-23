# Switch 死亡空间2 xbox360版 汉化工具

本仓库统一保存现有工程、工具、译稿、输入附件和操作步骤。归档日期：2026-10-24（北京时间）。

**v0.4 正式发布：用户已确认实机测试通过（2026-10-24）。**

实机通过状态依据用户反馈；未提供设备型号、游戏版本及逐项测试记录。原包内的“待实机验证”说明为发布前记录，最新状态见 [实机验证记录](docs/实机验证记录.md)。

## 下载

[正式发布页 v0.4](https://github.com/zqzhuhai-commits/DeadSpace2-Switch-Xbox360/releases/tag/v0.4)

- [v0.4 完整工具包（Windows EXE、源码、译稿、字体资源和说明）](https://github.com/zqzhuhai-commits/DeadSpace2-Switch-Xbox360/releases/download/v0.4/DS2_Localization_Tool_v04.zip)
- [最终翻译资源原包](https://github.com/zqzhuhai-commits/DeadSpace2-Switch-Xbox360/releases/download/v0.4/DS2_Chinese_Translation_Final.zip)
- [全部仓库文件 ZIP](https://github.com/zqzhuhai-commits/DeadSpace2-Switch-Xbox360/archive/refs/heads/main.zip)

## 文件导航

| 路径 | 内容 |
| --- | --- |
| `current/v0.4/` | 最新工具展开目录，含 EXE、Source、Translation、Replace、Reports、Licenses |
| `translation-final/` | 用户最终翻译包展开目录，保持其独立版本 |
| `archives/` | 已找回的各阶段原始 ZIP，保留历史版本 |
| `inputs/` | 本项目已提供的双盘资源附件、原始 8CA715C3、NRO 和两份 NSP |
| `docs/步骤与进度.md` | 制作进度、构建、修改、安装、恢复和验收步骤 |
| `docs/文件清单.json` | 每个归档文件的大小与 SHA-256 |

## 当前范围

已提供语言文件含 6,894 条索引：5,253 条译文与原文不同，1,641 条保留署名、标识等原文，空译文为 0。此统计仅覆盖已提供的 LH2，不证明提取了游戏全部文本，语义未逐句终审。

v0.4 包已封装四套中文字库，提供变长资源合并、VIV 索引更新、自动备份与恢复。原最终翻译包与 v0.4 译稿存在 11 条术语统一记录，二者分别保留；安装优先使用 v0.4 整包，避免混搭资源。

双盘输入 ZIP 经检查仅包含少量提取资源，**并非完整 bigfile0.viv**。工具需要用户在本机选择匹配版本的完整 VIV。NRO/NSP 为原附件归档，其来源和运行兼容性未在本次验证。

## 开始使用

下载并解压 v0.4 工具包，先阅读其中的使用说明。在 Windows 64 位系统运行工具，分别处理 Disc 1 和 Disc 2 自己的完整 `bigfile0.viv`，游戏语言保持 English。

详细说明见 [操作步骤](docs/步骤与进度.md)、[v0.4 使用说明](current/v0.4/使用说明.txt)、[现有检查结论](current/v0.4/检查报告.txt)。

字体许可见 [Noto Sans SC OFL](current/v0.4/Licenses/NotoSansSC_OFL.txt)。其他原附件按原样归档；本次不另行声明其许可证。
