# 🧠 知识笔记应用 — 14 天开发计划

```mermaid
mindmap
  root((知识笔记应用<br/>14 天开发计划))
    Week1_基础核心
      Day1【环境+项目骨架】
        技术栈选型
        CMake 构建
        基本窗口框架
      Day2【SQLite 存储层】
        笔记 CRUD
        Schema 设计
        迁移机制
      Day3【全文搜索+倒排索引】
        分词器
        倒排索引构建
        搜索高亮
      Day4【Markdown 编辑器】
        实时预览
        语法高亮
        快捷键支持
      Day5【目录树+标签面板】
        文件夹管理
        标签系统
        侧边栏布局
      Day6【双链系统】
        [[wiki 链接]]
        反向链接面板
        自动补全
      Day7【MVP 集成+自测】
        功能联调
        端到端流程
        Bug 收集
    Week2_进阶打磨
      Day8【闪卡复习 SM-2】
        SM-2 算法实现
        间隔重复调度
        复习界面
      Day9【版本历史 diff】
        Git 式版本管理
        Diff 可视化
        回滚支持
      Day10【Graph View 导图+图谱】
        节点关系图谱
        力导向布局
        导图视图
      Day11【UI 打磨+动画】
        过渡动画
        主题切换
        响应式布局
      Day12【三平台编译验证】
        Windows 构建
        macOS 构建
        Linux 构建
      Day13【测试+Bug 修复】
        单元测试
        集成测试
        回归修复
      Day14【CPack 打包发布】
        安装包制作
        版本号管理
        发布脚本
    已砍功能
      WebDAV 同步
      插件系统
      自定义存储引擎
      网页剪藏
      OCR
```

---

## 📋 项目概要

| 项目 | 细节 |
|------|------|
| **总周期** | 14 天（2 周） |
| **技术栈** | CMake + SQLite + Markdown |
| **定位** | 本地知识笔记应用 |
| **发布方式** | CPack 三平台打包 |
| **工作路径** | `E:\Codex\knowledge-notes` |
| **GitHub** | [Sunflower20-ui/knowledge-notes](https://github.com/Sunflower20-ui/knowledge-notes.git) |

---

## 📅 Week 1 — 基础核心

| 天数 | 模块 | 内容 |
|------|------|------|
| Day 1 | 环境 + 项目骨架 | 技术栈选型、CMake 构建、基本窗口框架 |
| Day 2 | SQLite 存储层 | 笔记 CRUD、Schema 设计、迁移机制 |
| Day 3 | 全文搜索 + 倒排索引 | 分词器、倒排索引构建、搜索高亮 |
| Day 4 | Markdown 编辑器 | 实时预览、语法高亮、快捷键支持 |
| Day 5 | 目录树 + 标签面板 | 文件夹管理、标签系统、侧边栏布局 |
| Day 6 | 双链系统 | [[wiki 链接]]、反向链接面板、自动补全 |
| Day 7 | MVP 集成 + 自测 | 功能联调、端到端流程、Bug 收集 |

## 📅 Week 2 — 进阶打磨

| 天数 | 模块 | 内容 |
|------|------|------|
| Day 8 | 闪卡复习 SM-2 | SM-2 算法实现、间隔重复调度、复习界面 |
| Day 9 | 版本历史 diff | Git 式版本管理、Diff 可视化、回滚支持 |
| Day 10 | Graph View | 节点关系图谱、力导向布局、导图视图 |
| Day 11 | UI 打磨 + 动画 | 过渡动画、主题切换、响应式布局 |
| Day 12 | 三平台编译验证 | Windows、macOS、Linux 构建 |
| Day 13 | 测试 + Bug 修复 | 单元测试、集成测试、回归修复 |
| Day 14 | CPack 打包发布 | 安装包制作、版本号管理、发布脚本 |

---

## 🗑️ 已砍功能

- **WebDAV 同步** — 暂不实现云同步
- **插件系统** — 核心优先，暂不扩展
- **自定义存储引擎** — SQLite 已足够
- **网页剪藏** — 非 MVP 必需
- **OCR** — 图片识别暂缓
