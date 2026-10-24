This snapshot contains 1,335 observed Aurora pipeline definitions. It contains
pipeline configuration rather than game assets and covers observed scenes only.
The snapshot combines the previously validated 1,309 definitions with 26 additional
definitions captured from a completed Final Battle winning run.

The adjacent manifest pins its size and SHA-256. CMake validates the hash and
stages both files beside the native executable. Normal startup validates them,
lets Aurora merge the definitions into the user's existing cache, and shares the
immutable snapshot with browser guests. Each GPU compiles definitions locally.

This exact snapshot passed preparation before readiness, synchronized start and
signed reconnect checks with four local Edge browsers, native Host plus three
Edge guests, and browser Host plus native Join and two Edge guests. These were
Windows tests on one computer; they do not establish other-device, mobile, WAN
or complete scene coverage. Unseen scene configurations may still compile during
gameplay.

When expanding coverage, generate a closed cache with the current Aurora schema,
validate its rows and actual native/browser preparation, then update both files
together. The current cache does not justify a claim of full shader coverage.
