# 开发

开发前请熟读 xv6-riscv-book ，有不确定的地方务必仔细配套原文参考代码，不轻易相信网络上的二手资料以及 AI 的回答。

开发完成功能后，要求测试以及详细的开发文档（最好写成教程一样详细，详细描述开发过程中的每一处考量），之后才可提交 PR。

# 协作

AI 参考

### 一、核心概念：为什么需要同步协作？

1.  **中央仓库**：通常是一个远程仓库（如 GitHub, GitLab, Gitee），作为代码的“真理之源”，所有开发者都与之同步。
2.  **本地仓库**：每个开发者电脑上的仓库。
3.  **分支**：Git 的杀手级功能，允许在不影响主线的同时进行开发。
    *   `main`/`master` 分支：稳定、可部署的代码。
    *   `develop` 分支：集成最新开发成果的分支。
    *   **功能分支**：为某个新功能或修复创建的分支。

---

### 二、基础同步命令

这是每个开发者都必须掌握的日常操作。

| 命令 | 作用 | 使用场景 |
| :--- | :--- | :--- |
| `git clone <url>` | **克隆**远程仓库到本地 | 第一次获取项目代码时 |
| `git pull` | **拉取**远程最新变更并**合并**到当前分支 | 开始工作前，同步最新代码 |
| `git fetch` | **获取**远程所有最新变更，但**不合并** | 查看别人做了什么，但不立即更新你的代码 |
| `git push` | **推送**本地提交到远程仓库 | 完成本地工作后，分享给他人 |

**`git pull` 的完整形式**：
```bash
git pull origin <branch-name>
# 等价于
git fetch origin && git merge origin/<branch-name>
```

---

### 三、常见的协作工作流程

#### 1. 集中式工作流

最适合小团队或初学者。

*   **流程**：
    1.  所有人克隆中央仓库。
    2.  直接在 `main` 分支上 `git pull` 更新。
    3.  完成工作后，直接 `git push` 到 `main` 分支。
*   **缺点**：容易产生冲突，代码稳定性难以保证。

#### 2. 功能分支工作流 - **最常用、最推荐**

所有新功能或修复都在专门的分支上完成。

*   **流程**：
    1.  开始新功能前，从 `main` 分支创建一个新分支：`git checkout -b feature/awesome-feature`
    2.  在新分支上开发、提交。
    3.  开发完成后，**推送到远程**：`git push -u origin feature/awesome-feature` (`-u` 是为了建立追踪)
    4.  在 Git 平台（如 GitHub）上发起 **Pull Request** 或 **Merge Request**。
    5.  团队成员在 PR/MR 中进行**代码审查**。
    6.  审查通过后，由负责人将分支合并到 `main`。
    7.  删除功能分支。

#### 3. Git Flow - 更复杂、更规范

适用于有固定发布周期的大型项目。

*   **主要分支**：
    *   `master`：与生产环境代码完全一致。
    *   `develop`：最新的开发状态。
*   **辅助分支**：
    *   `feature/*`：从 `develop` 拉出，合并回 `develop`。
    *   `release/*`：从 `develop` 拉出，用于测试和小修，分别合并到 `master` 和 `develop`。
    *   `hotfix/*`：从 `master` 拉出，用于紧急线上修复，分别合并到 `master` 和 `develop`。

有很多 GUI 工具和插件（如 `git-flow`) 可以简化此流程。

---

### 四、解决冲突

冲突是协作的必然产物，当两个人修改了同一文件的同一区域时就会发生。

**解决步骤**：

1.  **发现冲突**：在 `git pull` 或 `git merge` 时，Git 会提示 `CONFLICT (content)`。
2.  **定位冲突文件**：`git status` 会显示哪些文件是 `Both modified`。
3.  **手动解决**：打开冲突文件，你会看到类似这样的标记：
    ```diff
    <<<<<<< HEAD
    这是你本地修改的内容
    =======
    这是远程拉下来的内容
    >>>>>>> commit-hash
    ```
    *   删除 `<<<<<<<`, `=======`, `>>>>>>>` 这些标记。
    *   保留你想要的代码，或者将两部分修改整合成一段合理的代码。
4.  **标记为已解决**：
    ```bash
    git add <resolved-file>
    ```
5.  **完成合并**：
    ```bash
    git commit
    ```
    （如果是因为 `pull` 产生的冲突，这一步会自动完成）

---

### 五、最佳实践与黄金法则

1.  **勤提交，少推送**：
    *   本地提交可以频繁且细小，记录你的工作进度。
    *   但推送到远程的代码应该是**完整、可运行、经过测试**的。

2.  **在 Push 前先 Pull**：
    *   **永远**在执行 `git push` 之前，先执行 `git pull`（或 `git fetch + git rebase`）来将远程最新变更整合到本地。这能最大限度地减少冲突。

3.  **使用有意义的提交信息**：
    *   **坏例子**：`fix bug`
    *   **好例子**：`修复用户登录时因空指针导致的闪退问题`

4.  **善用 `.gitignore` 文件**：
    *   不要把编译产物、本地配置文件、IDE 文件、依赖库等提交到仓库。

5.  **代码审查**：
    *   Pull Request 是提高代码质量、分享知识的绝佳方式。不要跳过它。

6.  **保持分支的简洁和专注**：
    *   一个分支只做一件事（一个功能或一个修复）。完成后及时合并并删除。

---

### 六、典型协作场景示例

假设你要为一个项目添加一个新功能：

```bash
# 1. 获取最新代码（在main分支上）
git checkout main
git pull origin main

# 2. 基于main创建功能分支
git checkout -b feature/user-profile

# ...（进行开发，并多次 commit）...

# 3. 开发完成，推送到远程
git push -u origin feature/user-profile
# 现在去 GitHub 上创建 Pull Request

# 4. （如果PR期间，main分支有更新，需要同步以避免冲突）
git checkout main
git pull origin main
git checkout feature/user-profile
git rebase main
# 解决可能出现的冲突...
git push -f origin feature/user-profile # 由于rebase改变了历史，需要强制推送

# 5. PR 被合并后，清理本地分支
git checkout main
git pull origin main # 确保拉取到合并后的代码
git branch -d feature/user-profile # 删除本地功能分支
```

### 总结

Git 同步协作的核心在于：

*   **沟通**：通过 Issues, PR/MR 与团队沟通。
*   **规范**：遵循团队约定的工作流（如功能分支流）。
*   **纪律**：勤同步、写注释、做审查。

掌握这些，你就能在团队中高效、顺畅地进行协作开发了。



# 格式化

代码风格参照 xv6-riscv 内核代码风格。
