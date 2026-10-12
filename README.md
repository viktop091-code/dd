# Open Convoy

Open Convoy is an original DayZ server mod for configurable moving vehicle events. It does not contain code, routes, layouts, names, or assets from Triumph Moving AI Convoy.

## Current prototype

Version `0.1.0` provides:

- server-side JSON settings under `$profile:OpenConvoy/settings.json`;
- one or more vehicles per convoy;
- waypoint following with throttle, steering, braking, and manual gear changes;
- Expansion AI mercenary crews with a configurable basic loadout;
- convoy halt and AI dismount when a vehicle is attacked;
- cleanup at the end of a route and scheduled respawning;
- an example configuration generated on first server start.

The generated configuration starts with `Enabled: false`. It contains a first-pass ChernarusPlus route along the south coast. Verify and adjust the points for your server, then enable it. Loot presets, notifications, and an admin UI are planned after the driving controller and Expansion AI seating have been tested on a DayZ server.

A reviewable copy of the generated configuration is available at `examples/settings.json`.

## Source layout

- `OpenConvoy/config.cpp` — addon and vehicle registration;
- `OpenConvoy/scripts/3_Game` — serializable configuration;
- `OpenConvoy/scripts/4_World` — vehicle controller and event manager;
- `OpenConvoy/scripts/5_Mission` — dedicated-server startup hook.

## Build and test

1. Install DayZ Expansion Core and DayZ Expansion AI on the server.
2. Pack the `OpenConvoy` directory as `OpenConvoy.pbo` with DayZ Tools Addon Builder. Use `OpenConvoy` as the PBO prefix.
3. Place the PBO in `@OpenConvoy/Addons/` and copy `mod.cpp` to `@OpenConvoy/`.
4. Start a dedicated server with CF, Expansion Core, Expansion AI, and `@OpenConvoy` in `-mod`. The custom vehicle class is networked, so clients must load the same mod. Stop the server after `settings.json` is generated.
5. Verify the route coordinates, set `Enabled` to `true`, and restart the server.
6. Check the script log for `[OpenConvoy]` and observe the route. Tune waypoint spacing, radius, and target speed before adding gameplay systems.

There is no DayZ runtime or Addon Builder in this repository, so the final compile and physics test must be done on a machine with DayZ Server and DayZ Tools installed.

The included build script creates an unsigned PBO. Before publishing on a server with signature verification enabled, sign it with your own private key using DayZ Tools and distribute the matching public `.bikey` and `.bisign` files.
