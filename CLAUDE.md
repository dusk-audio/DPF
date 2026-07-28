# DPF (Dusk Audio fork)

## This is a fork, and changes never go upstream

Every change lands in **dusk-audio/DPF** and nowhere else. DISTRHO/DPF is fetched from, never
written to: no pushes, no pull requests, no issues. Fork-specific work is not submitted upstream
even when it looks generally useful.

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
gh repo set-default dusk-audio/DPF
```

`.git/hooks/pre-push` refuses any push whose URL points at DISTRHO/DPF. Hooks do not survive a
fresh clone, so re-add it when setting up a new checkout.

## Pull requests

PRs are opened against `dusk-audio/DPF` `main`. This fork takes same-repo PRs only, which is why
the workflows trigger on push to `**` rather than on `pull_request`.

## CI

Three workflows, all triggered on push to any branch: `build.yml` (make, every C++ std mode, every
UI_TYPE), `cmake.yml` (cmake matrix incl. a native ARM Linux runner and three MSVC legs) and
`wayland.yml` (Wayland-only container build, X11 regression, clap-validator, pluginval + LV2
validation). Anything relying on external package repositories is deliberately kept out of them.
