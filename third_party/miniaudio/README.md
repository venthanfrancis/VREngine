# miniaudio

Verbatim `miniaudio.h` from version 0.11.23:
https://raw.githubusercontent.com/mackron/miniaudio/0.11.23/miniaudio.h

License: MIT-0 (full license included at the end of the header).
SHA-256: `7e4f3f13c8fe66df2080ac3dd12a89193e3c2463cb7f067c798abd7331cd8ee6`.
Vendored following the repository's single-header dependency convention.
Only the optional Audio backend compiles it. Resource manager, decoders,
encoders, generators and the fallback null device are disabled. Third-party
warnings are isolated to its own translation unit; AREngine uses /W4 /WX.
