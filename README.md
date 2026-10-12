# Wild-Fields

Wild-Fields is a DayZ server mod for configurable moving vehicle convoys with Expansion AI crews.

## Version 0.1.3

- complete convoy vehicles with wheels, doors, battery, radiator, spark plug, and fluids;
- Expansion AI loadouts through `ExpansionHumanLoadout` (default: `BanditLoadout`);
- delayed, retried AI seating before the convoy starts moving;
- automatic removal of an AI unit that cannot enter its assigned seat within 45 seconds;
- waypoint driving, manual gear changes, cleanup, and respawn;
- convoy halt and crew dismount after a player attack.

The existing settings location remains `$profile:OpenConvoy/settings.json` so servers upgrading from an earlier build keep their routes. Add this field to an existing settings file if it is missing:

```json
"AILoadout": "BanditLoadout"
```

The generated example starts with `Enabled: false`. Verify the route coordinates, set it to `true`, and restart the server.

## Installation

1. Install CF, DayZ Expansion Core, and DayZ Expansion AI on the server and clients.
2. Copy `@Wild-Fields` to the server and client mod directories.
3. Add `@Wild-Fields` to the server `-mod` list after the dependencies.
4. Start the server once, edit `$profile:OpenConvoy/settings.json`, enable the event, and restart.
5. Check the script log for `[Wild-Fields]` messages.

The included PBO is unsigned. Servers using signature verification must sign it with their own private key and distribute the matching `.bikey` and `.bisign` files.

The AI creation, loadout, delayed seating, and retry sequence is adapted from Triumph Moving AI Convoy with permission from Triumph. See `THIRD_PARTY_NOTICES.md`.
