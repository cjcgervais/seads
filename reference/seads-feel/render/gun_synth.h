#pragma once

#include <cmath>
#include <cstdint>

#include "render/audio_dsp.h"
#include "render/gun_audio.h"

// Real-time gun / combat SFX synths for SEADS.
// Cosmetic, render-side only — reads nothing from sim/control/input/weapon/combat.
// No heap allocation in render(), no <random>, no wall-clock.
// Firewall: only <cmath>, <cstdint>, render/audio_dsp.h, render/gun_audio.h.

namespace render {

// ---------------------------------------------------------------------------
// GunSynth — machine-gun stutter (cannon + MG layers)
// ---------------------------------------------------------------------------
struct GunSynth {
    static constexpr int kMaxVoices = 24;

    // One shot voice per crack. Cannon voices additionally ring a sub-bass sine
    // (sub_env/sub_phase) under the crack for weight; MG voices leave it at 0.
    struct ShotVoice {
        bool   active    = false;
        double env       = 0.0;   // crack amplitude
        double tau       = 0.020; // crack decay tau [s]
        double sub_env   = 0.0;   // sub-thump amplitude (cannon only; 0 for MG)
        double sub_phase = 0.0;   // sub-thump sine phase [0,1)
        WSsvf  bp{};              // band-pass filter
        bool   free      = false; // bypasses firing_gain_ (external one-shot;
                                  // the FLAK boom fires while set_firing(false)
                                  // keeps the aircraft's composite roar shut)
    };

    double sr_           = 22050.0;
    double cannon_rate_  = 36.0;   // [Hz] composite cannon battery rate
    double mg_rate_      = 38.0;   // [Hz] composite MG battery rate
    double cannon_phase_ = 0.0;    // phase accumulator [0,1)
    double mg_phase_     = 0.0;

    // Firing gain (one-pole slew toward target)
    bool   firing_target_ = false;
    double firing_gain_   = 0.0;   // current slewed gain [0,1]
    double attack_coef_   = 0.0;   // per-sample attack coefficient
    double release_coef_  = 0.0;   // per-sample release coefficient

    // Precomputed SVF f-parameter (2*sin(pi*fc/sr))
    double cannon_f_ = 0.0;
    double cannon_qc_ = 0.0;
    double mg_f_     = 0.0;
    double mg_qc_    = 0.0;
    double inv_sr_   = 0.0;

    // Voice pools (ring / round-robin)
    ShotVoice cannon_voices_[kMaxVoices]{};
    ShotVoice mg_voices_[kMaxVoices]{};
    int cannon_next_ = 0;
    int mg_next_     = 0;

    WSXor rng_{};  // deterministic, seeded in init

    void init(double sr, double cannon_rate_hz, double mg_rate_hz) {
        sr_           = sr;
        cannon_rate_  = cannon_rate_hz;
        mg_rate_      = mg_rate_hz;
        cannon_phase_ = 0.0;
        mg_phase_     = 0.0;
        firing_gain_  = 0.0;
        firing_target_= false;
        cannon_next_  = 0;
        mg_next_      = 0;
        inv_sr_       = 1.0 / sr;

        // One-pole attack/release coefficients
        attack_coef_  = 1.0 - std::exp(-1.0 / (kGunAttack  * sr));
        release_coef_ = 1.0 - std::exp(-1.0 / (kGunRelease * sr));

        // SVF precompute
        cannon_f_  = 2.0 * std::sin(kAudioPI * kGunCannonFc / sr);
        cannon_qc_ = 1.0 / kGunCannonQ;
        mg_f_      = 2.0 * std::sin(kAudioPI * kGunMGFc / sr);
        mg_qc_     = 1.0 / kGunMGQ;

        // Fixed seed for determinism
        rng_.s = 0xBEEF1234CAFE5678ull;

        // Clear voices
        for (int i = 0; i < kMaxVoices; ++i) {
            cannon_voices_[i] = ShotVoice{};
            cannon_voices_[i].tau = kGunCannonCrackTau;
            mg_voices_[i] = ShotVoice{};
            mg_voices_[i].tau = kGunMGCrackTau;
        }
    }

    void set_firing(bool firing) {
        firing_target_ = firing;
    }

    // Trigger a cannon shot voice (crack + sub-thump). External callers get a
    // FREE voice: it sounds regardless of the firing gate — the flak boom is
    // triggered per spawned round while set_firing(false) holds the aircraft's
    // scheduled roar shut (found 2026-08-28: the boom was synthesized, then
    // multiplied by a firing_gain_ that had decayed to 0 — "I cant hear the
    // gun"). The internal scheduler passes free=false, so the aircraft path is
    // bit-identical.
    // STAGE D (the distant AI flak): `gain` scales the voice's initial
    // envelopes, which is how a report two kilometres away arrives quiet.
    // DEFAULTED TO 1.0 and applied by MULTIPLICATION, so every pre-existing
    // call site -- the internal scheduler, the player's flak boom, the two
    // audio tests -- is bit-identical by construction.
    void trigger_cannon(bool free = true, double gain = 1.0) {
        ShotVoice& v = cannon_voices_[cannon_next_];
        cannon_next_ = (cannon_next_ + 1) % kMaxVoices;
        v.active    = true;
        v.env       = gain;
        v.tau       = kGunCannonCrackTau;
        v.sub_env   = gain;
        v.sub_phase = 0.0;
        v.bp        = WSsvf{};
        v.free      = free;
    }

    // Trigger an MG shot voice
    void trigger_mg() {
        ShotVoice& v = mg_voices_[mg_next_];
        mg_next_ = (mg_next_ + 1) % kMaxVoices;
        v.active = true;
        v.env    = 1.0;
        v.tau    = kGunMGCrackTau;
        v.bp     = WSsvf{};
    }

    void render(short* buf, int n) {
        const double target_level = firing_target_ ? 1.0 : 0.0;

        for (int i = 0; i < n; ++i) {
            // Slew firing gain toward target
            double coef = (target_level > firing_gain_) ? attack_coef_ : release_coef_;
            firing_gain_ += (target_level - firing_gain_) * coef;

            // Advance phase and trigger shots only while firing
            if (firing_target_) {
                // Cannon phase. Jitter the period by ±kGunJitter*0.5 so the
                // stutter is a natural brrrt, not a metronome. A negative phase
                // after the wrap is INTENTIONAL and safe (the "late" shot): the
                // increment is strictly positive and no re-trigger happens until
                // it climbs back to 1.0 — do NOT clamp it to 0 (that would drop
                // every late draw and bias the rate high; Fable P1 2026-07-13).
                cannon_phase_ += cannon_rate_ * inv_sr_;
                if (cannon_phase_ >= 1.0) {
                    double jitter = kGunJitter * (rng_.w() * 0.5);
                    cannon_phase_ -= 1.0 + jitter;
                    trigger_cannon(/*free=*/false);  // gated by firing_gain_
                }
                // MG phase (same jitter discipline).
                mg_phase_ += mg_rate_ * inv_sr_;
                if (mg_phase_ >= 1.0) {
                    double jitter = kGunJitter * (rng_.w() * 0.5);
                    mg_phase_ -= 1.0 + jitter;
                    trigger_mg();
                }
            }

            // Sum active cannon voices: band-passed crack + a sub-bass sine
            // thump (the "boom" body that distinguishes it from the MG rattle).
            // Two accumulators: gated voices ride firing_gain_ (the aircraft's
            // held-trigger envelope); FREE voices (external one-shots — the
            // flak boom) go straight to the master, or the gate that shuts the
            // plane's roar while manned silences them too.
            double mix = 0.0;       // gated: scaled by firing_gain_
            double mix_free = 0.0;  // free: full level regardless of the gate
            for (int v = 0; v < kMaxVoices; ++v) {
                ShotVoice& sv = cannon_voices_[v];
                if (!sv.active) continue;
                double noise = rng_.w();
                double bp_out = sv.bp.bp(noise, cannon_f_, cannon_qc_);
                double sub = std::sin(2.0 * kAudioPI * sv.sub_phase) * sv.sub_env;
                (sv.free ? mix_free : mix) +=
                    bp_out * sv.env + kGunCannonSubMix * sub;
                // Decay the crack + advance/decay the sub-thump.
                sv.env *= std::exp(-inv_sr_ / kGunCannonCrackTau);
                sv.sub_phase += kGunCannonSubFc * inv_sr_;
                if (sv.sub_phase >= 1.0) sv.sub_phase -= 1.0;
                sv.sub_env *= std::exp(-inv_sr_ / kGunCannonSubTau);
                // Retire only once BOTH the crack and the sub have died out.
                if (sv.env < 1e-4 && sv.sub_env < 1e-4) sv.active = false;
            }
            // Sum active MG voices
            for (int v = 0; v < kMaxVoices; ++v) {
                ShotVoice& sv = mg_voices_[v];
                if (!sv.active) continue;
                double noise = rng_.w();
                double bp_out = sv.bp.bp(noise, mg_f_, mg_qc_);
                mix += bp_out * sv.env;
                sv.env *= std::exp(-inv_sr_ / kGunMGCrackTau);
                if (sv.env < 1e-4) sv.active = false;
            }

            // Apply firing gain and master, then soft-clip (graceful saturation
            // on stacked cracks instead of a harsh hard-clip corner). Free
            // voices skip the firing gain only — master + clip still apply.
            const double s =
                soft_clip((mix * firing_gain_ + mix_free) * kGunMaster);
            buf[i] = static_cast<short>(s * 32767.0);
        }
    }
};

// ---------------------------------------------------------------------------
// CombatSfxSynth — polyphonic explosion + hit one-shots
// ---------------------------------------------------------------------------
struct CombatSfxSynth {
    // ROUND 5: 8 -> 24. A flak hit boom parks two ECHO voices up to 2.1 s
    // ahead (trigger_flak_boom) while the 7.5 Hz self-destruct curtain keeps
    // rotating the ring -- an 8-slot ring recycles in ~1.1 s of sustained
    // fire and would steal every echo before it ever sounded. 24 slots =
    // ~3.2 s of recycle at full cyclic, longer than the farthest echo's
    // predelay + life. Inactive voices cost one branch per sample.
    static constexpr int kMaxExpVoices = 24;
    static constexpr int kMaxHitVoices = 8;
    // FLAK STAGE C reload foley. Small pool ON PURPOSE: a reload fires exactly
    // two of these 4 s apart, so four is already three more than the mechanism
    // can ask for -- the ring only exists so a second gun (or a mashed edge)
    // can never starve the first.
    static constexpr int kMaxClankVoices = 4;

    struct ExpVoice {
        bool      active  = false;
        double    age     = 0.0;   // < 0 = parked on a predelay (flak echo)
        double    gain    = 1.0;   // per-voice level; 1.0 for every legacy path
        // Round 5b timbre: hollow distant crump = lower rumble cutoff + the
        // crack layer mixed nearly out. Legacy defaults reproduce the old
        // voice exactly (cutoff is applied at trigger time via lp.set).
        double    crack_mix = 1.0;
        WSOnePole lp{};    // low rumble low-pass
        WSsvf     crack{}; // initial crack band-pass
    };

    struct HitVoice {
        bool   active = false;
        double age    = 0.0;
        WSsvf  bp{};  // metallic ping band-pass
    };

    // ★ THE FLAK DRUM SWAP (Stage C). Lives HERE and not in GunSynth for the
    // 2026-08-28 boom lesson, from the other side: GunSynth's voices ride
    // firing_gain_, which set_firing(false) closes while the flak is manned --
    // the boom only survives there because it sets ShotVoice::free. This synth
    // has NO such gate (every voice is a pure one-shot), so non-gun foley
    // belongs in it and cannot be silenced by a gate it never sees.
    struct ClankVoice {
        bool   active = false;
        bool   heavy  = false;  // false = drum OFF (clank), true = drum ON
        double age    = 0.0;
        WSsvf  ring{};  // the bright strike
        WSsvf  body{};  // the mass under it
    };

    double sr_      = 22050.0;
    double inv_sr_  = 0.0;

    // Precomputed filter parameters
    double exp_crack_f_    = 0.0;
    double exp_crack_qc_   = 0.0;
    double hit_f_          = 0.0;
    double hit_qc_         = 0.0;

    double clank_ring_f_   = 0.0, clank_ring_qc_ = 0.0;
    double clank_body_f_   = 0.0, clank_body_qc_ = 0.0;
    double latch_ring_f_   = 0.0, latch_ring_qc_ = 0.0;
    double latch_body_f_   = 0.0, latch_body_qc_ = 0.0;

    ExpVoice   exp_voices_[kMaxExpVoices]{};
    HitVoice   hit_voices_[kMaxHitVoices]{};
    ClankVoice clank_voices_[kMaxClankVoices]{};
    int exp_next_ = 0;
    int hit_next_ = 0;
    int clank_next_ = 0;

    WSXor rng_{};

    void init(double sr) {
        sr_     = sr;
        inv_sr_ = 1.0 / sr;
        exp_next_ = 0;
        hit_next_ = 0;
        clank_next_ = 0;

        // Fixed seed for determinism
        rng_.s = 0xDEADBEEF01234567ull;

        // Precompute explosion low-pass: set via WSOnePole.set()
        // (Will set per-voice in trigger so we don't need to precompute here;
        // store cutoff for use there.)

        // Explosion crack SVF
        exp_crack_f_  = 2.0 * std::sin(kAudioPI * kExplosionCrackFc / sr);
        exp_crack_qc_ = 1.0 / 4.0;  // Q=4 for the crack band

        // Hit ping SVF
        hit_f_  = 2.0 * std::sin(kAudioPI * kHitPingFc / sr);
        hit_qc_ = 1.0 / kHitPingQ;

        // Reload foley SVFs (Stage C)
        clank_ring_f_  = 2.0 * std::sin(kAudioPI * kClankRingFc / sr);
        clank_ring_qc_ = 1.0 / kClankRingQ;
        clank_body_f_  = 2.0 * std::sin(kAudioPI * kClankBodyFc / sr);
        clank_body_qc_ = 1.0 / kClankBodyQ;
        latch_ring_f_  = 2.0 * std::sin(kAudioPI * kLatchRingFc / sr);
        latch_ring_qc_ = 1.0 / kLatchRingQ;
        latch_body_f_  = 2.0 * std::sin(kAudioPI * kLatchBodyFc / sr);
        latch_body_qc_ = 1.0 / kLatchBodyQ;

        // Clear voices
        for (int i = 0; i < kMaxExpVoices; ++i) exp_voices_[i] = ExpVoice{};
        for (int i = 0; i < kMaxHitVoices; ++i) hit_voices_[i] = HitVoice{};
        for (int i = 0; i < kMaxClankVoices; ++i)
            clank_voices_[i] = ClankVoice{};
    }

    void trigger_explosion() {
        start_explosion(0.0, 1.0, kExplosionFc, 1.0);
    }

    // FLAK ROUND 5: the burst's own report. One boom voice now, and (echo)
    // the same voice re-fired off the valley walls -- kFlakEchoCount repeats
    // parked on negative ages, each quieter (gun_audio.h dials). A parked
    // voice renders silence until its clock crosses zero: explosion_amp is
    // 0 at age <= 0 by contract (test-pinned), so the predelay costs no
    // branch in render().
    //
    // Round 5b: `hollow` = the distant timbre (kFlakCrumpFc rumble, crack
    // nearly out -- Chad: "not so sharp poppy harsh but earthy"). Echoes are
    // ALWAYS hollow: a reflection has crossed the valley twice.
    void trigger_flak_boom(double gain, bool echo, bool hollow) {
        start_explosion(0.0, gain, hollow ? kFlakCrumpFc : kExplosionFc,
                        hollow ? kFlakCrumpCrackMix : 1.0);
        if (!echo) return;
        for (int e = 0; e < kFlakEchoCount; ++e)
            start_explosion(-kFlakEchoDelayS[e], gain * kFlakEchoGain[e],
                            kFlakCrumpFc, kFlakCrumpCrackMix);
    }

    void start_explosion(double start_age, double gain, double lp_fc,
                         double crack_mix) {
        ExpVoice& v = exp_voices_[exp_next_];
        exp_next_ = (exp_next_ + 1) % kMaxExpVoices;
        v.active  = true;
        v.age     = start_age;
        v.gain    = gain;
        v.crack_mix = crack_mix;
        v.lp      = WSOnePole{};
        v.lp.set(lp_fc, sr_);
        v.crack   = WSsvf{};
    }

    void trigger_hit() {
        HitVoice& v = hit_voices_[hit_next_];
        hit_next_ = (hit_next_ + 1) % kMaxHitVoices;
        v.active  = true;
        v.age     = 0.0;
        v.bp      = WSsvf{};
    }

    // heavy=false: the empty drum comes OFF (bright, short).
    // heavy=true:  the fresh drum SEATS and the latch closes (low, longer).
    void trigger_clank(bool heavy) {
        ClankVoice& v = clank_voices_[clank_next_];
        clank_next_ = (clank_next_ + 1) % kMaxClankVoices;
        v.active = true;
        v.heavy  = heavy;
        v.age    = 0.0;
        v.ring   = WSsvf{};
        v.body   = WSsvf{};
    }

    void render(short* buf, int n) {
        for (int i = 0; i < n; ++i) {
            double mix = 0.0;

            // Explosion voices
            for (int v = 0; v < kMaxExpVoices; ++v) {
                ExpVoice& ev = exp_voices_[v];
                if (!ev.active) continue;
                double amp = explosion_amp(ev.age);
                double noise = rng_.w();
                double rumble = ev.lp.lp(noise) * amp;
                double crack_contrib = 0.0;
                if (ev.age > 0.0 && ev.age < kExplosionCrackTime) {
                    // Initial crack: additional band-pass layer
                    double crack_noise = rng_.w();
                    crack_contrib = ev.crack.bp(crack_noise, exp_crack_f_, exp_crack_qc_)
                                    * amp * (1.0 - ev.age / kExplosionCrackTime)
                                    * ev.crack_mix;
                }
                mix += (rumble + crack_contrib) * kExplosionMaster * ev.gain;
                ev.age += inv_sr_;
                if (ev.age >= kExplosionAudioLife) ev.active = false;
            }

            // Hit voices
            for (int v = 0; v < kMaxHitVoices; ++v) {
                HitVoice& hv = hit_voices_[v];
                if (!hv.active) continue;
                double amp   = hit_amp(hv.age);
                double noise = rng_.w();
                double ping  = hv.bp.bp(noise, hit_f_, hit_qc_) * amp;
                mix += ping * kHitMaster;
                hv.age += inv_sr_;
                if (hv.age >= kHitAudioLife) hv.active = false;
            }

            // Reload foley voices (Stage C). One noise source struck through
            // two band-passes: a bright ring plus the mass under it. The
            // ENVELOPE and the two centre frequencies are the whole difference
            // between "lifted a hollow can off a bracket" and "dropped 27 kg
            // into a latch".
            for (int v = 0; v < kMaxClankVoices; ++v) {
                ClankVoice& cv = clank_voices_[v];
                if (!cv.active) continue;
                const double life = cv.heavy ? kLatchLifeS : kClankLifeS;
                const double tau  = cv.heavy ? kLatchDecayTau : kClankDecayTau;
                const double amp  = clank_amp(cv.age, life, tau);
                const double noise = rng_.w();
                const double ring =
                    cv.ring.bp(noise, cv.heavy ? latch_ring_f_ : clank_ring_f_,
                               cv.heavy ? latch_ring_qc_ : clank_ring_qc_);
                const double body =
                    cv.body.bp(noise, cv.heavy ? latch_body_f_ : clank_body_f_,
                               cv.heavy ? latch_body_qc_ : clank_body_qc_);
                mix += (ring * 0.75 + body) * amp *
                       (cv.heavy ? kLatchMaster : kClankMaster);
                cv.age += inv_sr_;
                if (cv.age >= life) cv.active = false;
            }

            // Soft-clip and write — a furball stacks many explosion + hit voices
            // at once; the soft limiter saturates gracefully instead of the old
            // hard-clip corner (which distorts the overshoot).
            buf[i] = static_cast<short>(soft_clip(mix) * 32767.0);
        }
    }
};

}  // namespace render
