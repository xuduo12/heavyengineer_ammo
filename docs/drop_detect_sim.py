import math

FS = 1000.0          # 反馈率 1kHz
WIN = 6              # 滑动平均窗口
FC = 10.0            # 低通截止
ALPHA = 1.0 - math.exp(-2.0 * math.pi * FC / FS)

def chain(raw):
    """复刻 Library/Src/motor.c:DJ_motor_speed_filter"""
    n = len(raw)
    ma = [0.0] * n
    lp = [0.0] * n
    buf = [0.0] * WIN
    s = 0.0
    idx = 0
    cnt = 0
    init = False
    lp_init = False
    out_ma = 0.0
    out_lp = 0.0
    for k, r in enumerate(raw):
        if not init:
            buf = [r] * WIN
            s = r * WIN
            idx = 0
            cnt = WIN
            init = True
        else:
            if cnt >= WIN:
                s -= buf[idx]
            buf[idx] = r
            s += r
            idx = (idx + 1) % WIN
            if cnt < WIN:
                cnt += 1
        out_ma = s / cnt
        if not lp_init:
            out_lp = out_ma
            lp_init = True
        else:
            out_lp += ALPHA * (out_ma - out_lp)
        ma[k] = out_ma
        lp[k] = out_lp
    return ma, lp

TARGET = 5200.0      # 实际设定值 (set2)
THRESH = 5000.0 - 20.0   # Ammo.c 里的触发门限: ammo_target_abs-20

print(f"ALPHA = {ALPHA:.4f}  (fc={FC}Hz @ {FS}Hz)")
print(f"触发门限 |speed| < {THRESH}  -> 需要掉速 {TARGET-THRESH:.0f} rpm")
print()
print(f"{'掉速D(rpm)':>10} {'脉宽T(ms)':>10} {'raw最低':>10} {'MA6最低':>10} {'LP最低':>10} {'过滤后掉幅':>12} {'能触发?':>8}")

for D in (100, 200, 300, 500):
    for Tms in (0.5, 1, 2, 5, 10):
        n = int(FS * 0.1)
        raw = [TARGET] * n
        start = int(FS * 0.02)
        w = max(1, int(FS * Tms / 1000.0))
        for k in range(start, start + w):
            raw[k] = TARGET - D
        ma, lp = chain(raw)
        seg = slice(start, start + max(w, 50))
        rawmin = min(raw[start:start + w])
        mamin = min(ma[seg])
        lpmin = min(lp[seg])
        print(f"{D:>10} {Tms:>10} {rawmin:>10.0f} {mamin:>10.1f} {lpmin:>10.1f} "
              f"{TARGET-lpmin:>12.1f} {'YES' if abs(lpmin) < THRESH else 'NO':>8}")

# 二级门限(3500-20)下的情况
print()
print("=== 一级(3500rpm, 门限3480, 需掉速20rpm) 噪声/纹波误触发检查 ===")
import random
random.seed(1)
for ripple in (5, 10, 20, 30):
    raw = [3500.0 + random.uniform(-ripple, ripple) for _ in range(5000)]
    ma, lp = chain(raw)
    lp = lp[100:]
    below = sum(1 for v in lp if abs(v) < 3480.0)
    # 计算最长连续 >=3480 的样本数
    run = best = 0
    for v in lp:
        if abs(v) >= 3480.0:
            run += 1
            best = max(best, run)
        else:
            run = 0
    print(f"  原始纹波 ±{ripple:>2} rpm: 低通最低={min(lp):.1f}, "
          f"低于3480占比={100.0*below/len(lp):.1f}%, 最长连续达标={best} 拍 "
          f"(需>=5拍才算打完一发)")

# ============================================================
# 新方案: 1kHz 中断里峰值保持 dip_peak = |speed_lp| - |speed_rpm|
#         2ms 任务里判 dip_peak > ammo_dip_trig_rpm
# ============================================================
print()
print("=== 新方案(相对基线 + 1kHz峰值保持) ===")
print(f"{'掉速D(rpm)':>10} {'脉宽T(ms)':>10} {'dip_peak':>10} {'>100rpm?':>10}")
for D in (50, 100, 200, 300, 500):
    for Tms in (0.5, 1, 2, 5, 10):
        n = int(FS * 0.1)
        raw = [TARGET] * n
        start = int(FS * 0.02)
        w = max(1, int(FS * Tms / 1000.0))
        for k in range(start, start + w):
            raw[k] = TARGET - D
        _, lp = chain(raw)
        peak = 0.0
        for k in range(0, n, 2):                      # 2ms 任务周期取走峰值
            block = max(abs(lp[j]) - abs(raw[j])
                        for j in range(k, min(k + 2, n)))
            if block > peak:
                peak = block
        print(f"{D:>10} {Tms:>10} {peak:>10.1f} "
              f"{'YES' if peak > 100.0 else 'no':>10}")
