# DPF (Dusk Audio)

## Hard fork: nothing goes out, nothing comes in

Forked from DISTRHO/DPF at `4238e1c7` (2025-10-23) and developed independently since. The
relationship is severed in **both** directions, deliberately:

- **Outbound:** no pushes, no pull requests, no issues on DISTRHO/DPF. Fork-specific work is not
  submitted upstream even when it looks generally useful.
- **Inbound:** never `git merge upstream/*` or `git pull upstream`. This tree is owned here, and an
  upstream merge would drag in code that has not been through this repository's CI gates.

`upstream` stays configured for one reason only: read-only reconnaissance. `git log
upstream/develop -- <file>` is useful when a bug turns up in inherited code, and their issue
tracker is worth reading for host-quirk reports. Anything worth having from there gets **written
here by hand** as our own commit, never cherry-picked as theirs. Note upstream develops on
`develop`; their `main` is a batch branch that sat still for the nine months after the fork point.

Remotes are set up to match:

| remote     | repository        | push                                    |
|------------|-------------------|-----------------------------------------|
| `origin`   | dusk-audio/DPF    | yes, the default for every branch        |
| `upstream` | DISTRHO/DPF       | no, push URL disabled + `pre-push` hook  |

`remote.pushDefault` is `origin` and `gh` resolves to `dusk-audio/DPF`, so `git push`, `gh pr
create` and `gh run list` all target the fork without arguments. If a clone ever lacks this, run:

```sh
git remote rename origin upstream && git remote rename dusk origin   # if inherited the old layout
git remote set-url --push upstream DISABLED-push-to-origin-instead
git config remote.pushDefault origin
git config --add remote.upstream.fetch '+refs/heads/develop:refs/remotes/upstream/develop'
gh repo set-default dusk-audio/DPF
```

`.git/hooks/pre-push` refuses any push whose URL points at DISTRHO/DPF. Hooks do not survive a
fresh clone, so re-add it when setting up a new checkout.

## Pull requests

PRs are opened against `dusk-audio/DPF` `main`. This repository takes same-repo PRs only, which is
why the workflows trigger on push to `**` rather than on `pull_request`.

**Never against DISTRHO/DPF.** Two were opened there by accident (#533, #534, both closed within a
minute) because GitHub's post-push banner and the VS Code GitHub extension default a new PR's base
to the *parent of the fork network*, which no git config can override. Guards in place:

- `.vscode/settings.json` restricts the VS Code extension to the `origin` remote and stops it
  offering a PR after every push.
- The repository left DISTRHO's fork network on 2026-07-28, so DISTRHO is no longer a selectable
  base anywhere in the UI: `gh api repos/dusk-audio/DPF --jq '{fork,parent}'` reports
  `{"fork": false, "parent": null}`. This is a standalone repository now, not a fork of anything.
  `git fetch upstream` is unaffected, as expected — the fork network and git remotes are unrelated.
  If a future clone ever reports `"fork": true` again, something was re-created from the wrong
  place.

## CI

Three workflows, all triggered on push to any branch: `build.yml` (make, every C++ std mode, every
UI_TYPE), `cmake.yml` (cmake matrix incl. a native ARM Linux runner and three MSVC legs) and
`wayland.yml` (Wayland-only container build, X11 regression, clap-validator, pluginval + LV2
validation). Anything relying on external package repositories is deliberately kept out of them.
