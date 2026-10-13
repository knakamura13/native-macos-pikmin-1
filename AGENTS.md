# Working in this fork

This fork (`knakamura13/native-macos-pikmin-1`) tracks the upstream project `SSunnKing/Open-Nectar---Pikmin-Native-PC-Mobile-Port`. The upstream remote is named `upstream`.

## Branches

- `main` mirrors `upstream/main`. Change it only by fast-forward. Never commit to it.
- `kyle-develop` is `main` plus personal changes. It is pushed to `origin`.
- `fix/<name>` holds one upstream-bound change. It branches from `main`. Its PR goes to upstream.

Never rebase `kyle-develop` or force-push it. Merge into it.

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
