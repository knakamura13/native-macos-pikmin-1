# Working in this fork

This fork (`knakamura13/native-macos-pikmin-1`) tracks the upstream project `SSunnKing/Open-Nectar---Pikmin-Native-PC-Mobile-Port`. The upstream remote is named `upstream`.

## Branches

- `main` mirrors `upstream/main`. Change it only by fast-forward. Never commit to it.
- `kyle-develop` is `main` plus personal changes. It is pushed to `origin`.
- `fix/<name>` holds one upstream-bound change. It branches from `main`. Its PR goes to upstream.

Never rebase `kyle-develop` or force-push it. Merge into it.

## What goes upstream

Goal: a Mac Pikmin 1 that matches GameCube as closely as possible. Classify each change first:

1. GameCube fidelity: original logic, content, progression, timing, and unlocks. Original bugs stay, unless a mouse or keyboard feature cannot work without the fix.
2. Port bugfixes: fixes for bugs the port introduced. Upstream PR.
3. Input features: features that exist because of the mouse or keyboard, such as cursor-aimed throw. Upstream PR, active only while that device drives the game. Controller players keep GameCube behavior.
4. Upstream-introduced features (Whistle Pluck, Infinite Day, No Day Limit, Hide Olimar's Messages): fix, support, and enhance them upstream. They have no input gate. Removing one stays fork-only.
5. Fork-only additions: `kyle-develop` only.

Bugs:

- A bug the upstream introduced: fix it, and open the upstream PR from a `fix/<name>` branch cut from `main` (synced to `upstream/main`).
- A platform-agnostic fix covers every platform in one change. Before claiming it works on all platforms, compile it on Windows and Linux.
- A macOS-only fix that certainly also affects Linux or Windows: open the upstream PR first, then one upstream issue per affected OS. Each issue links the PR and claims no fix for its OS.

## Sync `main` with upstream

```sh
git fetch upstream
git switch main
git merge --ff-only upstream/main
git push origin main
```

## Start an upstream fix

```sh
git switch main
git switch -c fix/<name>
```

Keep the change minimal. Do not include `kyle-develop` work. Open the PR upstream with head `knakamura13:fix/<name>`.

## Bring a fix into `kyle-develop`

```sh
git switch kyle-develop
git merge fix/<name>
```

## After an upstream PR merges

1. Sync `main` (see above).
2. Merge `main` into `kyle-develop`:

   ```sh
   git switch kyle-develop
   git merge main
   ```

   If a conflict is only wording, keep `main`'s version. The code is the same on both sides.
3. Delete the `fix/<name>` branch locally and on `origin`.

## Personal changes

Commit them on `kyle-develop`. Prefer new files, or code behind a build flag that is off by default. Avoid edits to shared files such as `pc_window.cpp` and `pc_settings.cpp` where you can, because upstream changes them often.

## Checks

- macOS app: `games/pikmin1/packaging/macos/build-app.sh`. Add `--install` to replace `/Applications/Open Nectar.app`. Quit the game first.
- Windows and Linux builds are not run on this Mac. Before opening an upstream PR, confirm that the change compiles there, or say in the PR that it was not built.
