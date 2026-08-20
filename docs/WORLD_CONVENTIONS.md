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

## Cross Product Reference (added in M1)

This is a non-obvious consequence of the convention above, worth stating
explicitly instead of leaving someone to guess it: in a standard
right-handed system, `X × Y = Z`, `Y × Z = X`, `Z × X = Y` — but that
rule applies to the raw `+X`/`+Y`/`+Z` axes, not to the `Right`/`Up`/
`Forward` *names*, because `Forward` is defined as `-Z`, not `+Z`.

Concretely, with `Right = +X = (1,0,0)` and `Up = +Y = (0,1,0)`:

```
Cross(Right, Up) = X × Y = +Z = -Forward   (NOT +Forward)
```

`AREngine::Core::Math::Cross` is tested against this exact result in
`tests/core_tests.cpp` — see that test for the worked derivation. Do not
assume `Cross(Right, Up)` points forward; it points backward.
