This snapshot contains 618 observed Aurora shader definitions, not game assets or
complete scene coverage. It comes from the validated local gameplay cache retained
as `browser-full-match-618-pipeline-seed.db` in the project validation outputs.

The adjacent manifest pins its size and SHA-256. CMake validates the hash and
stages both files beside the native executable. Normal startup validates them,
lets Aurora merge the definitions into the user's existing cache, and shares the
immutable snapshot with browser guests. Each GPU compiles definitions locally.

When expanding coverage, generate a closed cache with the current Aurora schema,
validate its rows and actual native/browser preparation, then update both files
together. The current cache does not justify a claim of full shader coverage.
