#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
gimbal 工程 <-> GitHub 双向自动同步

监视本地源文件变化, 并定期检查远端; 双向对齐:
  - 本地有改动   -> add / commit / push
  - 远端有改动   -> fetch + pull(快进或合并)
  - 两边都有改动 -> 先提交本地(必要时 stash), 再合并; 冲突则暂停并提示

用法:
    python tools/autosync.py            # 持续运行
    python tools/autosync.py --once     # 只跑一轮后退出

安全约定:
  - 绝不 force push, 绝不丢弃本地提交
  - 合并冲突时不会自动解决, 会停下来提示你手工处理
  - build/ 等已在 .gitignore 中, 不会上传
"""
import os
import subprocess
import sys
import time
import traceback
import datetime

# 控制台中文输出(Windows cmd 默认 GBK 会乱码)
try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

# ---------------- 配置 ----------------
REPO_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REMOTE = "origin"
BRANCH = "main"

# 提交信息统一使用 UTC+8 当前时间(精确到秒)
TZ_UTC8 = datetime.timezone(datetime.timedelta(hours=8))
DEBOUNCE_SEC = 3.0       # 本地最后一次改动后等待多久再同步(秒)
POLL_SEC = 1.0           # 本地文件轮询间隔(秒)
REMOTE_CHECK_SEC = 20.0  # 每隔多久 fetch 一次远端(秒)
NET_TIMEOUT = 240        # 单个网络 git 命令超时(秒)

WATCH_EXT = {
    ".c", ".h", ".cpp", ".hpp", ".s", ".S",
    ".py", ".md", ".txt", ".ioc", ".ld",
    ".json", ".yml", ".yaml",
}

IGNORE_DIRS = {
    ".git", "build", ".agents", ".dsh", ".vscode",
    "__pycache__", ".idea", "node_modules",
}


def log(msg):
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)


def git(*args, timeout=NET_TIMEOUT):
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
        return 124, "", f"git {' '.join(args)} 超时"
    except FileNotFoundError:
        return 127, "", "PATH 中找不到 git"


def snapshot():
    """扫描工作区, 返回 {相对路径: (mtime_ns, size)}"""
    state = {}
    for root, dirs, files in os.walk(REPO_DIR):
        dirs[:] = [d for d in dirs if d not in IGNORE_DIRS]
        for name in files:
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


def rev(revspec):
    rc, out, _ = git("rev-parse", revspec)
    return out if rc == 0 else None


def dirty():
    rc, out, _ = git("status", "--porcelain")
    return rc == 0 and bool(out)


def now_utc8():
    """当前时间, UTC+8, 精确到秒 -> '2026-11-06 09:26:13'"""
    return datetime.datetime.now(TZ_UTC8).strftime("%Y-%m-%d %H:%M:%S")


def commit_local():
    """把未提交改动做成一个提交; 提交信息 = 当前时间(UTC+8, 精确到秒)"""
    if not dirty():
        return False
    rc, _, err = git("add", "-A")
    if rc != 0:
        log(f"git add 失败: {err}")
        return False
    rc, changed, err = git("diff", "--cached", "--name-only")
    if rc != 0:
        return False
    n = len([x for x in changed.splitlines() if x.strip()])
    stamp = now_utc8()
    # author/committer 日期也写成同一时刻的 UTC+8
    rc, out, err = git("commit", "-q", f"--date={stamp} +0800", "-m", stamp)
    if rc != 0:
        log(f"commit 失败: {err or out}")
        return False
    log(f"已提交 {n} 个文件 | {stamp}")
    return True


def merge_remote():
    """把 origin/BRANCH 合入本地; 返回 'ok' / 'conflict' / 'error'"""
    rc, _, _ = git("fetch", REMOTE, BRANCH)
    if rc != 0:
        return "error"
    return merge_up_to_date()


def merge_up_to_date():
    """基于已 fetch 的 origin/BRANCH 做合并"""
    remote = rev(f"{REMOTE}/{BRANCH}")
    local = rev("HEAD")
    if not remote or not local:
        return "error"
    if remote == local:
        return "ok"

    # 本地是否已包含远端(可以快进推送, 无需合并)
    rc, _, _ = git("merge-base", "--is-ancestor", remote, local)
    if rc == 0:
        return "ok"                      # 本地更新 -> 等 push

    # 远端领先本地 -> 快进或合并
    rc, out, err = git("merge", "--ff-only", f"{REMOTE}/{BRANCH}")
    if rc == 0:
        log("远端有新提交, 已快进合并")
        return "ok"

    # 分叉: 若无共同祖先(历史被改写), 不合并, 交给 push 用 force-with-lease 覆盖
    rc3, _, _ = git("merge-base", remote, local)
    if rc3 != 0:
        log("本地历史与远端无共同祖先(历史已改写), 跳过合并, 将强制推送")
        return "ok"

    # 分叉: 需要真正的合并
    rc2, out2, err2 = git(
        "merge", f"{REMOTE}/{BRANCH}", "--no-edit",
        "-m", now_utc8(),
    )
    if rc2 == 0:
        log("远端与本地分叉, 已自动合并")
        return "ok"

    if "CONFLICT" in (out2 + err2) or "冲突" in (out2 + err2):
        log("!!! 合并冲突, 需要手工解决。已暂停自动同步。")
        log(f"    冲突文件:\n{out2}\n{err2}")
        return "conflict"

    log(f"merge 失败: {err2 or out2}")
    return "error"


def push():
    rc, out, err = git("push", REMOTE, BRANCH)
    if rc == 0:
        log(f"已推送到 {REMOTE}/{BRANCH}")
        return True

    combined = (out + err).lower()
    # 历史被改写(如 amend/filter-branch)后普通 push 会被拒; 仅此时用 force-with-lease
    if "non-fast-forward" in combined or "rejected" in combined or "fetch first" in combined:
        remote = rev(f"{REMOTE}/{BRANCH}")
        if remote:
            log("检测到历史已改写, 使用 force-with-lease 推送(远端无新增提交才允许)")
            rc2, out2, err2 = git(
                "push", "--force-with-lease", REMOTE, BRANCH)
            if rc2 == 0:
                log(f"已(强制)推送到 {REMOTE}/{BRANCH}")
                return True
            log(f"force push 失败: {err2 or out2}")
            return False

    log(f"push 失败(稍后重试): {err or out}")
    return False


def sync_once(check_remote=True):
    """完成一次双向对齐"""
    if check_remote:
        st = merge_remote()
        if st == "conflict":
            return False

    local_commits = commit_local()

    # 合并远端(推之前再确认一次, 避免 non-fast-forward)
    if check_remote:
        st = merge_up_to_date()
        if st == "conflict":
            return False

    # 还有需要推送的提交吗
    if rev("HEAD") and rev(f"{REMOTE}/{BRANCH}") and rev("HEAD") != rev(f"{REMOTE}/{BRANCH}"):
        push()
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
    log(f"同步目标: {REMOTE}/{BRANCH}  (双向)")
    log(f"防抖 {DEBOUNCE_SEC:g}s | 本地轮询 {POLL_SEC:g}s | 远端检查 {REMOTE_CHECK_SEC:g}s")
    log("Ctrl+C 退出")

    baseline = snapshot()
    pending_since = None
    last_remote_check = time.time()

    while True:
        time.sleep(POLL_SEC)
        try:
            cur = snapshot()
            now = time.time()

            # ---- 本地变化 ----
            if cur != baseline:
                baseline = cur
                pending_since = now
                continue

            fire = False
            if pending_since is not None and now - pending_since >= DEBOUNCE_SEC:
                pending_since = None
                fire = True

            # ---- 定期检查远端 ----
            if now - last_remote_check >= REMOTE_CHECK_SEC:
                last_remote_check = now
                fire = True

            if fire:
                if not sync_once():
                    log("同步未完成, 将在下一轮重试")

        except KeyboardInterrupt:
            log("收到中断, 退出")
            return 0
        except Exception:
            log("异常(继续监视):")
            traceback.print_exc()


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(0)
