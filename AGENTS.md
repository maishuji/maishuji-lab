# Agent instructions

This file defines the working agreement for coding agents in this repository.
Follow it together with the user's request and any more specific instructions
in subdirectories.

## Project context

maishuji-lab is an educational C++20 framework over public KallistiOS APIs for
the Sega Dreamcast. Keep Dreamcast concepts, costs, lifetimes, and hardware
boundaries visible. Read the relevant README and `docs/` files before changing
build, KOS, PVR, Flycast, or runtime behavior.

Maintain the root [`GLOSSARY.md`](GLOSSARY.md) as the concise source for
Dreamcast-, KOS-, and PVR-specific terminology used by the project. Add or
update an entry when a change introduces a term that may be unfamiliar or
ambiguous outside its hardware context. Keep entries short and pertinent, and
use this exact two-line form: the first line is
`<term>(<optional-scope>):`; the second starts with one tab followed by a
third-person verb-based definition, for example:

```text
DMA(pvr):
	Represents direct memory access used to move data without a CPU copy for every word.
```

Prefer the Dreamcast meaning when a term has multiple meanings, and update the
glossary in the same change as the documentation or code that introduces the
term.

## Git workflow

- Inspect the current branch and worktree before starting. Preserve unrelated
  user changes.
- Use Conventional Commits for every commit. The subject must follow
  `<type>(<scope>): <imperative summary>`; omit the scope when it adds no
  information. Examples: `docs(flycast): explain serial log capture` and
  `fix(pvr): reject submission after list finish`.
- Add a commit body when the change needs context, evidence, compatibility
  notes, or test details. Keep the subject concise and make the body explain
  the reason and verification, not repeat the diff.
- Commit each completed logical step when the user asks for implementation.
  Keep commits focused and leave the worktree clean at handoff when practical.
- Always commit with the repository's locally configured Git identity. Check it
  with:

  ```sh
  git config --local --get user.name
  git config --local --get user.email
  ```

  Do not replace it with a guessed name, a hard-coded `--author`, or a global
  identity. If the local identity is missing or unusable, report that before
  committing.
- Do not amend, rebase, force-push, reset, or discard user work unless the user
  explicitly requests that exact operation.
- Before committing, review the staged diff and run `git diff --cached --check`.
  Mention the commit hash and author in the handoff.

## Evidence and documentation

- Put durable findings in `docs/`. Documentation is part of the engineering
  output and must help both future agents and human users.
- When recording a finding, include the relevant version/commit, command or
  reproduction path, observed result, expected result, limitations, and any
  confidence or “not verified” boundary. Prefer short runnable examples over
  vague conclusions.
- Update an existing document when it is the natural source of truth; create a
  focused Markdown document when the topic deserves independent reuse.
- Keep user-facing instructions understandable without requiring an agent to
  read private planning files. Do not copy secrets, credentials, or sensitive
  local data into `docs/`.
- When external documentation or source was checked, link it near the claim and
  distinguish upstream facts from observations made in this repository.

## Private command trace

- Maintain `.private/agents_notes.md` for every task. This file is intentionally
  local-only and is ignored by Git; never force-add it.
- Append an entry for every shell command run, including build, test, Git,
  inspection, and diagnostic commands. Record the date/time, the exact command
  (or a faithful redacted form), exit status, and a concise result. Truncate
  large output, but retain the lines that establish success, failure, or the
  important observation.
- Never record passwords, tokens, private keys, or other secrets. Say
  `[redacted]` and explain what was omitted.
- Add a human-readable explanation whenever a command has several flags or
  non-obvious syntax. Explain what each meaningful flag does, especially flags
  such as `-a`, `-p`, `--cached`, `--check`, `-D`, `-j`, or `--filesystem`.
  Explain pipelines, redirections, environment variables, and destructive
  scope as well. The goal is that a human can understand what the command did
  without knowing the tool already.
- Update the notes promptly after the command, before moving to the next
  investigation step when practical. If a command is still running, record the
  start and final result when it finishes.
- Keep the notes useful but compact: summarize repeated output and retain full
  output only when it is needed to reproduce or diagnose the result.

Use this format:

```text
## YYYY-MM-DD HH:MM — short task/step name

Command: `...`
Flags and syntax: `--flag` means ...; `-p` means ...; the pipe/redirection does ...
Exit status: 0
Result: concise observation; include important output lines in a fenced block
when useful.
```

## Validation and handoff

- Validate in proportion to the change: documentation changes need formatting
  and link/diff checks; build or runtime changes need the relevant host,
  target, and emulator checks.
- Report what was run, what passed, and what was not possible. Do not claim
  real Dreamcast hardware validation when only a build or emulator was tested.
- For failures, preserve the useful diagnostic output in the private command
  trace and record the limitation in `docs/` when it affects users or future
  agents.
