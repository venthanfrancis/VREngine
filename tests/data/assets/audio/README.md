# Project-owned audio fixture

`tone.wav`: generated mathematically for AREngine, no sampled or copyrighted
recording. RIFF PCM16 mono, 48000 Hz, 12000 frames (0.25 seconds), 440 Hz sine.
Sample n is round(6000 * sin(2*pi*440*n/48000)). Used for deterministic decoder,
mixing, and device playback validation. No external audio content or attribution.
