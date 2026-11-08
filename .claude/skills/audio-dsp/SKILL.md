---
name: audio-dsp
description: DSP recipes and quality rules for the plugins in this repo — envelope followers, noise gates, lookahead limiters, compressors, smoothing, metering, denormals. Use when writing or reviewing any audio processing code.
---

# Audio DSP recipes

All reusable DSP lives header-only in `shared/dsp/`. Prefer extending it over re-implementing per plugin.

## Fundamentals
- One-pole coefficient for a time constant `t` seconds: `c = exp(-1 / (t * fs))`; update `y = x + c * (y - x)`.
- dB ↔ gain: `juce::Decibels::decibelsToGain / gainToDecibels` (use a floor, e.g. -100 dB).
- Recompute coefficients when parameters change (once per block is fine), never per sample with `exp` if avoidable.
- Stereo-link detectors (max of |L|,|R|) so the image doesn't shift.
- Everything multiplying audio is smoothed (`SmoothedValue`, 20–50 ms).

## Noise gate (`shared/dsp/NoiseGate.h`)
- Detector: peak follower, instant attack, ~20 ms release.
- Hysteresis: open at `thr`, close at `thr - hyst` (≈4 dB) → no chatter.
- Hold counter (samples) before closing.
- Gain smoother: attack coefficient when opening, release coefficient when closing. Smoothing linear gain with a one-pole gives a dB-linear fade, which sounds natural.
- Range: closed gain = `dbToGain(range)` (−80 dB ≈ off) so it can also act as an expander/ducker.

## Lookahead brickwall limiter (`shared/dsp/LookaheadLimiter.h`)
Lookahead `L` samples (≈1.5 ms). Per sample, stereo-linked:
1. required gain `r = min(1, ceiling / peak)`
2. `m` = sliding-window minimum of `r` over the last `L+1` samples (monotonic deque, O(1))
3. release: `s = m < s ? m : m + rel * (s - m)` (instant attack onto the held minimum)
4. `g` = moving average of `s` over `L` samples (smooth attack ramp)
5. output = delayed sample (`L` back) × `g`, then a safety hard clip at the ceiling.
Because the min-hold window ≥ the average window, `g` never exceeds the requirement of the sample leaving the delay → no overshoot. Report `L` as latency.

## Metering
- Audio thread writes peak values into `std::atomic<float>` using a "max since last read" pattern; UI reads with `exchange(0)` each frame.
- Gain reduction is reported as the minimum gain in the block.

## Testing
- Feed synthetic signals (sine bursts, silence, full-scale square) in a small console harness or the Standalone build.
- Check: no output above ceiling, no clicks at gate open/close, no NaN/denormal CPU spikes on silence, correct behaviour at 44.1–192 kHz.
