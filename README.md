# Hack Menu

A small Geode mod for Geometry Dash 2.2081 (Windows) with:

- **Noclip**: you can't die (the attempt is flagged as cheated, like any noclip).
- **Speedhack**: game speed from 0.1x to 10x, with matching music pitch if you want it.
- **Show hitboxes**: GD's own hitbox view, outside practice mode too.
- **Click buffer**: a jump click that would do nothing (mid-air on cube, ball,
  robot or spider, with no orb) is held for up to N physics steps and fires as
  soon as you land or touch an orb.
- **Survival delay**: each jump press or release is held until the first frame
  where it won't kill you (checked by simulating ahead from a checkpoint), up
  to N frames. Safe inputs go through right away. Meant for tight wave spam.
- **Frame advance**: freeze gameplay and step forward N physics steps at a time.
- **FPS bypass**: pick your own frame rate.
- **Safe mode**: nothing from the attempt is saved. Forced on while click
  buffer, survival delay, frame advance or speedhack is active, since GD can't detect those.
- **Cheat indicator**: "Cheating" / "Safe mode" label in the corner.

Open the menu with **F2**, or with the **Hacks** button in the pause menu.

| Key | Action |
|---|---|
| F2 | Open the menu |
| F6 | Toggle frame advance |
| F7 | Step forward (hold to repeat) |
| F8 | Toggle click buffer |
| F9 | Toggle survival delay |
Every option is also under Geode → Hack Menu → Settings.

## How each hack works

| Hack | Hook | Idea |
|---|---|---|
| Noclip | `PlayLayer::destroyPlayer` | Skip the death call unless it's GD's anticheat spike |
| Speedhack | `CCScheduler::update` | Multiply every frame's delta time; set FMOD master pitch |
| Hitboxes | `PlayLayer::postUpdate` | Call GD's built-in `updateDebugDraw()` every frame |
| Click buffer | `GJBaseGameLayer::processQueuedButtons` | Pull wasted jump pushes out of the input queue, replay them with `handleButton` when they'd work |
| Survival delay | `GJBaseGameLayer::update` | Pull jump inputs from the queue; for each, `createCheckpoint`, apply it, run the lookahead, `loadFromCheckpoint`; apply once it survives |
| Frame advance | `GJBaseGameLayer::update` | Skip updates while frozen; run one 240 Hz step per queued step |
| FPS bypass | `CCDisplayLinkDirector::setAnimationInterval` | Replace the interval GD asks for with our own |
| Safe mode | `PlayLayer::destroyPlayer`, `levelComplete` | Pretend to be a start position run (`m_isTestMode`), which GD never saves |

## Installing

Download this repo (Code → Download ZIP), unzip it, and double-click
`installer/install.bat`. It finds Geometry Dash through Steam (or asks you for
the folder), installs Geode if you don't have it, and puts the latest build
of the mod in `geode/mods`. Run it again any time to update.

The installer downloads from the `latest-build` release, which the workflow
rebuilds on every push to `main`.

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
