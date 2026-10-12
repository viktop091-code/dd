# Wild-Fields

Wild-Fields 1.0.0 is a complete DayZ moving convoy event based on the supplied Triumph Moving AI Convoy code, reused with the permission represented by the commissioning user.

The mod includes:

- working multi-vehicle convoy driving and following;
- Expansion AI crews with delayed, retried vehicle seating;
- M1025 and truck presets with assembled parts;
- seven bundled Chernarus routes;
- configurable crews, vehicles, routes, loot, timing, notifications, and cleanup;
- ambush behavior, crew dismounting, corpse handling, and kill-feed controls;
- route recording, admin menu, optional map markers, and up to three active convoy slots;
- DayZ 1.30 profile-directory compatibility handling.

## Requirements

- CF;
- DayZ Expansion Core;
- DayZ Expansion AI;
- matching client and server installation of `@Wild-Fields`.

## Installation

1. Remove the earlier prototype PBO and do not load `@OpenConvoy` or the original convoy PBO alongside this adaptation; they use overlapping internal classes.
2. Copy the complete `@Wild-Fields` directory to the server and clients.
3. Load `@Wild-Fields` after CF and Expansion dependencies.
4. Start the server. Defaults are created under `$profile:WildFields/`.
5. Edit `$profile:WildFields/WildFieldsSettings.json` and the JSON files in `Vehicles`, `Crews`, and `Routes` as needed.
6. Check the server script log for `[Wild-Fields]` messages.

The bundled defaults are enabled and schedule the first convoy after 300 seconds. The default routes are for Chernarus; replace them when using another map.

The PBO is unsigned. Servers using signature verification must sign it with their own private key and distribute the matching `.bikey` and `.bisign` files.

See `THIRD_PARTY_NOTICES.md`, plus `LICENSE.txt` and `PERMISSION_NOTICE.txt` inside the source/PBO.
