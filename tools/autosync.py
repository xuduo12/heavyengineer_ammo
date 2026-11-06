#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
gimbal 工程 -> GitHub 单向自动同步

监视本目录下的源文件变化，防抖后自动 add / commit / push 到 origin。

用法:
    python tools/autosync.py            # 持续运行
    python tools/autosync.py --once     # 只同步一次后退出

行为说明:
  - 单向: 只从本地推送到远端, 绝不会把远端内容拉回覆盖本地
  - 只提交本目录(仓库根)内的改动, 已被 .gitignore 排除的 build/ 等不会上传
  - 无改动时不产生空提交
  - push 失败(断网等)会保留本地提交, 并在下一轮自动重试
"""
import os
import subprocess
import sys
import time

# ---------------- 配置 ----------------
REPO_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REMOTE = "origin"
BRANCH = "main"
DEBOUNCE_SEC = 3.0      # 最后一次改动后等待多久再同步(秒)
POLL_SEC = 1.0          # 文件轮询间隔(秒)
FULL_RESCAN_TAIL = 2000  # 每轮抽查的文件数上限(防止大仓库卡顿)

WATCH_EXT = {
    ".c", ".h", ".cpp", ".hpp", ".s", ".S",
    ".py", ".md", ".txt", ".ioc", ".ld",
    ".json", ".yml", ".yaml", ".gitignore", ".mxproject",
}

IGNORE_DIRS = {
    ".git", "build", ".agents", ".dsh", ".vscode",
    "__pycache__", ".idea", "node_modules",
}
IGNORE_FILES = {".gitignore"}   # .gitignore 本身也要同步, 不在忽略列表


def log(msg):
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)


def git(*args, timeout=180):
    """执行 git 命令, 返回 (returncode, stdout, stderr)"""
    env = dict(os.environ, GIT_TERMINAL_PROMPT="0")
    try:
        p = subprocess.run(
            ["git", *args], cwd=REPO_DIR, capture_output=True,
            text=True, encoding="utf-8", errors="replace",
            timeout=timeout, env=env,
        )
        return p.returncode, p.stdout.strip(), p.stderr.strip()
    except subprocess.TimeoutExpired:
        return 124, "", f"git {' '.join(args)} timed out"
    except FileNotFoundError:
        return 127, "", "git not found in PATH"


def snapshot():
    """扫描工作区, 返回 {相对路径: (mtime_ns, size)}"""
    state = {}
    for root, dirs, files in os.walk(REPO_DIR):
        dirs[:] = [d for d in dirs if d not in IGNORE_DIRS]
        for name in files:
            if name in IGNORE_FILES:
                pass  # 仍然监视
            ext = os.path.splitext(name)[1]
            if ext not in WATCH_EXT and name not in (".gitignore", ".mxproject"):
                continue
            full = os.path.join(root, name)
            try:
                st = os.stat(full)
            except OSError:
                continue
            rel = os.path.relpath(full, REPO_DIR).replace("\\", "/")
            state[rel] = (st.st_mtime_ns, st.st_size)
    return state


def has_uncommitted():
    rc, out, _ = git("status", "--porcelain")
    return rc == 0 and bool(out)


def has_unpushed():
    rc, out, _ = git("rev-list", f"{REMOTE}/{BRANCH}..HEAD", "--count")
    return rc == 0 and out not in ("", "0")


def sync_once(reason="定时"):
    """提交并推送一次; 返回 True 表示动作成功或无事可做"""
    if not has_uncommitted() and not has_unpushed():
        return True

    if has_uncommitted():
        rc, out, err = git("add", "-A")
        if rc != 0:
            log(f"git add 失败: {err}")
            return False

        rc, changed, err = git("diff", "--cached", "--name-only")
        if rc != 0:
            log(f"git diff 失败: {err}")
            return False

        n = len([x for x in changed.splitlines() if x.strip()])
        stamp = time.strftime("%Y-%m-%d %H:%M:%S")
        msg = f"自动同步 {stamp} ({n} 个文件)"
        rc, out, err = git("commit", "-q", "-m", msg)
        if rc != 0:
            log(f"commit 失败: {err or out}")
            return False
        log(f"已提交 {n} 个文件: {msg}")

    rc, out, err = git("push", REMOTE, BRANCH, timeout=300)
    if rc != 0:
        log(f"push 失败(本地提交已保留, 稍后重试): {err or out}")
        return False
    log(f"已推送到 {REMOTE}/{BRANCH}")
    return True


def main():
    once = "--once" in sys.argv

    rc, _, _ = git("rev-parse", "--git-dir")
    if rc != 0:
        log(f"错误: {REPO_DIR} 不是 git 仓库")
        return 1

    if once:
        ok = sync_once()
        return 0 if ok else 1

    log(f"监视目录: {REPO_DIR}")
    log(f"目标: {REMOTE}/{BRANCH}  (单向: 本地 -> 远端)")
    log(f"防抖 {DEBOUNCE_SEC:g}s, 轮询 {POLL_SEC:g}s。Ctrl+C 退出。")

    baseline = snapshot()
    pending_since = None
    last_ok_state = None

    while True:
        time.sleep(POLL_SEC)
        try:
            cur = snapshot()

            if cur != baseline:
                # 有文件变化, 重置防抖计时
                baseline = cur
                pending_since = time.time()
                continue

            if pending_since is not None:
                quiet = time.time() - pending_since
                if quiet < DEBOUNCE_SEC:
                    continue
                pending_since = None
                sync_once()

        except KeyboardInterrupt:
            log("收到中断, 退出")
            return 0
        except Exception as e:          # 保持守护进程存活
            log(f"异常(已忽略, 继续监视): {e!r}")


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(0)
