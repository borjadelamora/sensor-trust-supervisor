# EXP-20260812-003-camera-blackout—findings

Every frame inside the declared window is black and every frame outside it is untouched. 615
frames of 2441 fall inside `[18.577, 33.577)` and all 615 were flooded, with nothing flooded
outside and nothing left half done.

## Result

The classification is exact in both directions. 615 frames inside the window, 615 flooded. 1826
frames outside, 1826 identical to their source byte for byte. Zero frames came out at some
intensity other than the one the scenario declares, which is the check that would catch a partial
write or a frame flooded from a stale buffer.

A flooded frame is still an image a monitor can read: 30 000 bytes, `rgb8`, 100 by 100, the same
`step` and `encoding` and dimensions as the frame it replaced, with only the payload set to zero.
That matters because the `Image.data` setter validates every element unless it is handed an
`array.array('B')`, and getting that wrong costs 3.9 ms a frame instead of 0.002 ms.

The attack began 12 ms after the drawn onset, which is inside one frame period at the rate the
camera was delivering. That is as close as this can land: the rule is evaluated per frame, so the
first frame at or past the onset takes it.

The frame count inside the window is worth reading carefully. 615 frames over a 15 s window is
41 Hz, against the 50 Hz the SDF declares, because the renderer does not keep up with simulated
time on this machine and drops frames while it catches up. So the number of frames a
15 s blackout contains is a property of the host rather than the scenario, and comparing runs by
that count would compare renderers. Comparing them by window membership does not.

## Numbers

| | value |
|---|---|
| Flight duration | 66.63 s |
| Drawn onset | 18.577 s |
| Delivered onset | 18.589 s |
| Declared window | [18.577, 33.577) s |
| Camera frames, twin | 2441 |
| Frames inside the window | 615 |
| Frames flooded | 615 |
| Frames outside the window | 1826 |
| Frames outside that were altered | 0 |
| Frames partly corrupted | 0 |
| Frame size after flooding | 30 000 bytes, rgb8, 100×100 |
| GPS fixes altered | 0 |
| Flow messages altered | 0 |
| Largest gap in PX4 stamps | 6.13 s |
| Stale stamps ignored | 4 |

## What I would change next time

Intensity 0 is the easy end. A saturated white frame at intensity 1 is the same code path and
worth one run, but the case a mean-brightness check would actually struggle with is a partial
loss, a frame that is dark rather than black, or one that freezes on the last good image. The
scenario schema takes an intensity already, so the first of those is a parameter change rather
than new code.

Nothing here measures detection. There is no camera monitor yet, so all this establishes is that
the frames arriving at one would be the frames the scenario says.
