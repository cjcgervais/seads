#!/usr/bin/env python3
# STRAND D-E input-map forensics. READ-ONLY on tapes.
# T record token map (test/harness/sled_tape.h, 54 tokens):
#  0 'T' | 1 tick | 2 throttle 3 brake 4 steer 5 lean_lat 6 lean_fwd 7 stand
#  | 8 air_temp 9 snow_hard 10 cold_ref | 11..51 pin41 | 52 surface 53 rolled
# pin offsets: +28 steer_actual(=39) +29 belt(=40) +30 rpm(=41) +35 gspeed(=46)
import sys, json, math

DT = 1.0 / 120.0


def scan(path):
    tick = []; thr = []; brk = []; st = []; ll = []; lf = []; sd = []
    sa = []; belt = []; gs = []
    n_over = 0; over_ticks = []
    with open(path, 'r', errors='replace') as f:
        for line in f:
            if not line:
                continue
            c = line[0]
            if c == 'T':
                p = line.split()
                if len(p) < 54:
                    continue
                tick.append(int(p[1]))
                thr.append(float(p[2])); brk.append(float(p[3])); st.append(float(p[4]))
                ll.append(float(p[5])); lf.append(float(p[6])); sd.append(float(p[7]))
                sa.append(float(p[39])); belt.append(float(p[40])); gs.append(float(p[46]))
            elif c == 'O':
                p = line.split()
                if len(p) >= 2:
                    n_over += 1; over_ticks.append(int(p[1]))
    return dict(tick=tick, thr=thr, brk=brk, st=st, ll=ll, lf=lf, sd=sd, sa=sa,
                belt=belt, gs=gs, n_over=n_over, over_ticks=over_ticks)


def pct(x, q):
    if not x:
        return None
    x = sorted(x); i = (len(x) - 1) * q / 100.0
    lo = int(math.floor(i)); hi = min(lo + 1, len(x) - 1)
    return x[lo] + (x[hi] - x[lo]) * (i - lo)


def analyse(d, name):
    st = d['st']; sa = d['sa']; ll = d['ll']; lf = d['lf']; thr = d['thr']
    N = len(st)
    out = {'file': name, 'n_ticks': N, 'duration_s': N * DT}
    if N < 10:
        return out

    # ---- frame cadence: a nonzero step in the held command marks a new frame
    gaps = []; last = None
    for i in range(1, N):
        if st[i] != st[i - 1]:
            if last is not None:
                gaps.append(i - last)
            last = i
    out['frame_gap_ticks_p50'] = pct(gaps, 50)
    out['frame_gap_ticks_p90'] = pct(gaps, 90)
    out['est_fps_from_steer'] = (1.0 / (pct(gaps, 50) * DT)) if gaps else None

    # ---- steer command: steady-state occupancy
    zero = sum(1 for v in st if abs(v) < 1e-9)
    lock = sum(1 for v in st if abs(v) >= 0.999)
    out['steer_cmd_exact_zero_frac'] = zero / N
    out['steer_cmd_full_lock_frac'] = lock / N
    out['steer_cmd_transit_frac'] = 1.0 - (zero + lock) / N

    # ---- steer excursions (leave 0, return to 0)
    exc = []; i = 0
    while i < N:
        if abs(st[i]) > 1e-9:
            j = i; pk = 0.0
            while j < N and abs(st[j]) > 1e-9:
                pk = max(pk, abs(st[j])); j += 1
            dur = (j - i) * DT
            plateau = sum(1 for k in range(i, j) if abs(abs(st[k]) - pk) < 1e-6) * DT
            exc.append((dur, pk, plateau))
            i = j
        else:
            i += 1
    if exc:
        out['steer_excursions'] = len(exc)
        out['steer_exc_per_min'] = len(exc) / (N * DT / 60.0)
        out['steer_exc_dur_p50'] = pct([e[0] for e in exc], 50)
        out['steer_exc_dur_p90'] = pct([e[0] for e in exc], 90)
        out['steer_exc_peak_p50'] = pct([e[1] for e in exc], 50)
        out['steer_exc_saturated_frac'] = sum(1 for e in exc if e[1] >= 0.999) / len(exc)
        out['steer_exc_held_partial'] = sum(1 for e in exc if e[1] < 0.98 and e[2] >= 0.25)
        out['steer_exc_plateau_p50'] = pct([e[2] for e in exc], 50)
        out['steer_exc_shorter_than_ramp_frac'] = sum(1 for e in exc if e[0] < 0.5) / len(exc)

    # ---- key decode from monotone runs: rate ~2.0 = key, ~3.0 = self-centre
    runs = []; i = 1
    while i < N:
        d0 = st[i] - st[i - 1]
        if abs(d0) < 1e-12:
            i += 1; continue
        s = 1 if d0 > 0 else -1
        j = i; tot = 0.0; t0 = i - 1
        while j < N and (st[j] - st[j - 1]) * s >= 0:
            tot += st[j] - st[j - 1]; j += 1
        dur = (j - 1 - t0) * DT
        if dur > 1e-9:
            runs.append((abs(tot) / dur, s, dur, abs(tot)))
        i = j
    key_runs = [r for r in runs if 1.4 < r[0] < 2.6]
    rel_runs = [r for r in runs if 2.6 <= r[0] < 3.8]
    out['runs_total'] = len(runs)
    out['runs_rate_p50'] = pct([r[0] for r in runs], 50)
    out['runs_near_2p0_frac'] = len(key_runs) / len(runs) if runs else None
    out['runs_near_3p0_frac'] = len(rel_runs) / len(runs) if runs else None
    out['key_press_est'] = len(key_runs)
    out['key_press_per_min'] = len(key_runs) / (N * DT / 60.0) if runs else None
    out['key_hold_dur_p50'] = pct([r[2] for r in key_runs], 50)
    out['key_hold_dur_p90'] = pct([r[2] for r in key_runs], 90)
    out['key_hold_lt_0p5s_frac'] = (sum(1 for r in key_runs if r[2] < 0.5) / len(key_runs)) if key_runs else None

    # ---- command vs bar (steer_actual): lag and overshoot of the command
    lag = [abs(st[i] - sa[i]) for i in range(N)]
    out['cmd_minus_bar_abs_p50'] = pct(lag, 50)
    out['cmd_minus_bar_abs_p90'] = pct(lag, 90)
    out['cmd_minus_bar_abs_max'] = max(lag)
    dec = [i for i in range(1, N) if abs(st[i]) < abs(st[i - 1]) - 1e-12]
    out['ticks_cmd_decaying'] = len(dec)
    if dec:
        out['bar_behind_during_decay_p90'] = pct([abs(st[i] - sa[i]) for i in dec], 90)
    out['bar_full_lock_frac'] = sum(1 for v in sa if abs(v) >= 0.999) / N
    out['bar_transit_frac'] = sum(1 for v in sa if 0.02 < abs(v) < 0.98) / N

    # ---- lean: band dwell, drift, reversals, C recentre
    out['lean_band_0p3_0p7_frac'] = sum(1 for v in ll if 0.3 <= abs(v) <= 0.7) / N
    out['lean_gt_0p98_frac'] = sum(1 for v in ll if abs(v) > 0.98) / N
    out['lean_exact_zero_frac'] = sum(1 for v in ll if abs(v) < 1e-9) / N
    out['lean_abs_mean'] = sum(abs(v) for v in ll) / N
    dw = []; i = 0
    while i < N:
        if 0.3 <= abs(ll[i]) <= 0.7:
            j = i
            while j < N and 0.3 <= abs(ll[j]) <= 0.7:
                j += 1
            dw.append((j - i) * DT); i = j
        else:
            i += 1
    if dw:
        out['lean_band_dwell_p50'] = pct(dw, 50); out['lean_band_dwell_p90'] = pct(dw, 90)
        out['lean_band_dwell_max'] = max(dw); out['lean_band_entries'] = len(dw)
        out['lean_band_entries_per_min'] = len(dw) / (N * DT / 60.0)
    c_hits = sum(1 for i in range(1, N)
                 if abs(ll[i]) < 1e-12 and abs(lf[i]) < 1e-12 and abs(st[i]) < 1e-12
                 and (abs(ll[i - 1]) > 0.02 or abs(lf[i - 1]) > 0.02))
    out['C_recentre_est'] = c_hits
    out['C_recentre_per_min'] = c_hits / (N * DT / 60.0)
    rev = sum(1 for i in range(1, N) if ll[i] * ll[i - 1] < 0)
    out['lean_sign_reversals_per_min'] = rev / (N * DT / 60.0)
    srev = sum(1 for i in range(1, N) if st[i] * st[i - 1] < 0)
    out['steer_sign_reversals_per_min'] = srev / (N * DT / 60.0)

    # ---- thumb / CVT band
    out['thr_zero_frac'] = sum(1 for v in thr if v < 1e-9) / N
    out['thr_wot_frac'] = sum(1 for v in thr if v > 0.999) / N
    out['thr_mid_frac'] = sum(1 for v in thr if 1e-9 <= v <= 0.999) / N
    cb = sum(1 for i in range(N) if thr[i] < 1e-9 and d['belt'][i] < 3.25)
    out['cvt_freecoast_frac'] = cb / N
    out['thr_closed_but_engaged_frac'] = sum(
        1 for i in range(N) if thr[i] < 1e-9 and d['belt'][i] >= 3.25) / N
    hold = []; i = 0
    while i < N:
        if 0.05 < thr[i] < 0.95:
            j = i
            while j < N and 0.05 < thr[j] < 0.95:
                j += 1
            hold.append((j - i) * DT); i = j
        else:
            i += 1
    if hold:
        out['thr_mid_dwell_p50'] = pct(hold, 50); out['thr_mid_dwell_p90'] = pct(hold, 90)
        out['thr_mid_dwell_max'] = max(hold)
        out['thr_mid_entries_per_min'] = len(hold) / (N * DT / 60.0)
    out['n_override'] = d['n_over']
    out['override_per_min'] = d['n_over'] / (N * DT / 60.0)
    return out


if __name__ == '__main__':
    res = []
    for p in sys.argv[1:]:
        try:
            d = scan(p)
            res.append(analyse(d, p.replace('\\', '/').split('/')[-1]))
            sys.stderr.write('ok %s\n' % p)
        except Exception as e:
            sys.stderr.write('FAIL %s %s\n' % (p, e))
    print(json.dumps(res, indent=1))
