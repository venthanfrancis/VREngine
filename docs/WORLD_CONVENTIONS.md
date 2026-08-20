# AREngine World Conventions

These are fixed decisions. Retrofitting them later would touch every
module that does math, so they are locked in before any real math code
is written (M1).

## Distance Unit

```
1.0 engine unit = 1 meter
```

AR requires real-world scale — virtual content has to line up with the
physical world — so the engine works in meters natively rather than an
arbitrary "game unit."

## Coordinate System

```
Handedness: right-handed
Up:         +Y
Forward:    -Z
Right:      +X
```

This is a conventional right-handed, Y-up setup (matching common tools
and math libraries). All view/projection matrix construction, cross
products, and camera math in `Core` and `Rendering` must agree with this
convention.

## Scope

This applies engine-wide: `Scene` transforms, `Input`/`XR` poses,
`Rendering` camera and projection setup, and any content pipeline
(`Assets`) that imports data from external tools — importers must convert
into this convention at load time if the source format differs (e.g.
many DCC tools default to Z-up or left-handed).
