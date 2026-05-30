<p align="center">
  <img src="resources/icons/app_icon.svg" width="96" height="96" alt="KnowledgeNotes">
</p>

<h1 align="center">KnowledgeNotes</h1>
<p align="center"><strong>本地知识笔记应用</strong> · 2 周从零构建</p>

<p align="center">
  <img src="https://img.shields.io/badge/C++-17-blue?logo=c%2B%2B" alt="C++17">
  <img src="https://img.shields.io/badge/Qt-6.11-green?logo=qt" alt="Qt 6.11">
  <img src="https://img.shields.io/badge/CMake-3.21-blue?logo=cmake" alt="CMake">
  <img src="https://img.shields.io/badge/SQLite-FTS5-orange?logo=sqlite" alt="SQLite+FTS5">
  <img src="https://img.shields.io/badge/license-MIT-lightgrey" alt="MIT">
</p>

---

## 📖 简介

KnowledgeNotes 是一款**本地优先**的知识笔记应用，类似于 Obsidian / Notion 的轻量替代，专为快速记录、组织和回顾知识设计。

- **Markdown 编辑**，实时预览 + 语法高亮
- **双链系统**（`[[wiki link]]`），反向链接面板
- **全文搜索**（基于 SQLite FTS5 + BM25 排序）
- **目录树** + 标签面板管理笔记
- **SM-2 闪卡复习**，间隔重复记忆
- **Graph View** 知识图谱可视化
- **版本历史** diff 对比与回滚
- **三平台** Windows / macOS / Linux 支持

---

## 🛠 技术栈

| 层级 | 技术 |
|------|------|
| 语言 | C++17 |
| 框架 | Qt 6.11（Core / Gui / Widgets / Sql） |
| 构建 | CMake ≥ 3.21 |
| 数据库 | SQLite 3 + FTS5 全文搜索 + WAL 模式 |
| 打包 | CPack（Day 14） |

---

## 🚀 快速开始

### 环境要求

- **Qt 6.11**（推荐通过 Qt Online Installer 安装，选择 MinGW 64-bit 套件）
- **CMake 3.21+**
- **编译器**：MSVC 2019+ / MinGW 64-bit / GCC 9+ / Clang 12+

### 克隆 & 构建

```bash
git clone https://github.com/Sunflower20-ui/knowledge-notes.git
cd knowledge-notes
mkdir build && cd build
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

Windows 用户也可以用 Qt Creator 打开 `CMakeLists.txt` 直接编译。

### 运行

构建完成后，可执行文件位于 `build/src/knowledge_notes`（或 `.exe`）。

首次运行会自动在项目 `data/` 目录下创建 `knowledge_notes.db`。

---

## 📁 项目结构

```
knowledge-notes/
├── CMakeLists.txt              # 根构建配置
├── PROJECT_PLAN.md             # 14 天开发计划
├── resources/
│   ├── resources.qrc           # Qt 资源清单
│   ├── style.qss               # Tokyo Night 深色主题
│   └── icons/                  # SVG 图标集
├── src/
│   ├── CMakeLists.txt
│   ├── main.cpp                # 入口，数据库初始化
│   ├── app/
│   │   ├── main_window.h/cpp   # 主窗口（三栏布局）
│   └── core/
│       ├── database.h/cpp      # SQLite 连接 / WAL / Schema 迁移
│       ├── note_repository.h/cpp # CRUD / 标签 / 双链 / FTS 搜索
│       └── search_result.h     # 搜索结果结构体
├── tests/
└── data/
    └── knowledge_notes.db      # 运行时自动创建
```

---

## 📅 开发路线图（14 天）

### Week 1 — 基础核心

| 天 | 模块 | 状态 |
|----|------|:----:|
| Day 1 | 环境搭建 + 项目骨架 + 三栏布局 | ✅ |
| Day 2 | SQLite 存储层：笔记 CRUD、标签、双链 | ✅ |
| Day 3 | FTS5 全文搜索 + 倒排索引 + BM25 排序 | ✅ |
| Day 4 | Markdown 编辑器：实时预览、语法高亮 | ⬜ |
| Day 5 | 目录树 + 标签面板 | ⬜ |
| Day 6 | 双链系统：`[[wiki link]]`、反向链接、自动补全 | ⬜ |
| Day 7 | MVP 集成 + 端到端自测 | ⬜ |

### Week 2 — 进阶打磨

| 天 | 模块 | 状态 |
|----|------|:----:|
| Day 8 | 闪卡复习 SM-2 算法 | ⬜ |
| Day 9 | 版本历史 diff + 回滚 | ⬜ |
| Day 10 | Graph View 知识图谱 | ⬜ |
| Day 11 | UI 打磨 + 过渡动画 | ⬜ |
| Day 12 | 三平台编译验证 | ⬜ |
| Day 13 | 测试 + Bug 修复 | ⬜ |
| Day 14 | CPack 打包发布 | ⬜ |

### 已砍功能

- WebDAV 同步
- 插件系统
- 自定义存储引擎
- 网页剪藏
- OCR

---

## 🎨 主题

采用 **Tokyo Night** 深色配色方案：

| 用途 | 色值 |
|------|------|
| 主背景 | `#1a1b26` |
| 版面 | `#1f2335` |
| 表面 | `#24283b` |
| 强调蓝 | `#7aa2f7` |
| 琥珀高亮 | `#e0af68` |

---
