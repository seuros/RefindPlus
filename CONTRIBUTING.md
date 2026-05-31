# Contributing

Meridian is firmware code. Keep changes narrow, reviewable, and tied to one
behavioral goal.

## Before You Change Code

Run:

```sh
./scripts/check.sh
```

Install local commit hooks once:

```sh
pre-commit install
```

If you are changing build metadata, also run:

```sh
./scripts/build.sh --workspace /path/to/edk2 --dry-run
```

## Change Rules

- Prefer the EDK2 package files as the source of truth.
- Do not reintroduce inherited subdirectory makefiles. EDK2 package metadata is
  the source of truth for built files.
- Keep imported/vendor code separate from local Meridian behavior.
- Do not mass-reformat unrelated files.
- Preserve original copyright and license provenance.
- Add tests or host-side checks for parsers, path handling, and launch policy
  whenever code can be exercised outside firmware.

## Style

Follow the surrounding C style for touched code until a project-wide formatter
policy is adopted. Small consistency edits in touched functions are fine;
tree-wide whitespace churn is not.

## Review Focus

For firmware changes, reviewers should be able to answer:

- Which build target includes this code?
- Which configuration token or runtime condition activates it?
- What happens on Apple firmware, generic UEFI firmware, and OpenCore
  chain-loads?
- What is the failure mode if the relevant protocol, file, or NVRAM variable is
  missing?
