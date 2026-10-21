# Hack Menu

A small Geode mod for Geometry Dash 2.2081 (Windows) with:

- **Noclip**: you can't die (the attempt is flagged as cheated, like any noclip).
- **Speedhack**: game speed from 0.1x to 10x, with matching music pitch if you want it.
- **Show hitboxes**: GD's own hitbox view, outside practice mode too.

Open the menu with **Tab**, or with the **Hacks** button in the pause menu.
Every option is also under Geode → Hack Menu → Settings.

## How each hack works

| Hack | Hook | Idea |
|---|---|---|
| Noclip | `PlayLayer::destroyPlayer` | Skip the death call unless it's GD's anticheat spike |
| Speedhack | `CCScheduler::update` | Multiply every frame's delta time; set FMOD master pitch |
| Hitboxes | `PlayLayer::postUpdate` | Call GD's built-in `updateDebugDraw()` every frame |

## Building

### In the cloud (no setup)

Push this folder to a GitHub repo. The workflow in `.github/workflows/build.yml`
builds it on every push. Download `hack-menu` from the run's Artifacts, unzip it,
and drop the `.geode` file into `Geometry Dash/geode/mods`.

### Locally

1. Install Visual Studio 2022 with **Desktop development with C++**, plus CMake.
2. Install the [Geode CLI](https://docs.geode-sdk.org/getting-started/geode-cli), then run
   `geode sdk install` and `geode sdk install-binaries`.
3. In this folder run `geode build`. The `.geode` file is installed into your GD automatically.

## Customizing

Change `id` and `developer` in `mod.json` to your own name before sharing it.
