# VoxLord

A voxel sandbox and, fine, a Minecraft clone. I can't afford the real thing 😭😭 and I don't want to pirate it 🏴‍☠️, so this is my own blocks game. You dig, you build, you poke around, and if you're lazy you hand the controls to an AI and watch it play.

## What it is

A blocks world that actually behaves, written in C++.

Terrain is generated from a seed. The same seed always gives the same world, and a new seed rolls a fresh one. Lakes and oceans sit in the valleys, mountains climb up, caves riddle the ground, and trees and structures scatter around. Breaking a block and placing another updates the lighting live, so opening a wall lets sunlight pour in and walling yourself in makes it go properly dark.

You carry a hotbar instead of an inventory. Scroll or hit the number row to switch what you're placing. Day and night run on a cycle, and glow blocks stay lit through the dark, which you'll want when you're underground.

Physics don't clip. You walk, run, jump, swim, and sink in fluid, and there's a capsule collider underneath so you hop a one-block lip instead of snagging on every seam. Lava is lava. Don't step in it.

It's also built for weak machines. Vsync is on, loading threads stay light, and `VIEW_DISTANCE` and `LOADING_WORKERS_COUNT` are build options if you want to push further. If it runs on a potato, so be it.

## Multiplayer

Solo is fun for a while. Then you want a second person.

`voxlord-server` runs headless and hosts a world over TCP for a bunch of people. Or use host-and-play, where you make a world and friends join straight off you, no separate server box. The server is the source of truth for where everyone stands. Clients predict ahead and correct, so nobody teleports around on your screen. Since every client builds the identical world from the shared seed, there's no chunk-streaming lag. The server just runs physics and checks your edits for distance, cooldown, and reach.

## AI player control (MCP)

The fun bit. VoxLord has a built-in MCP server called `voxlord-mcp` (protocol version `2025-03-26`), so an AI agent can plug into the game's port and actually play. A bot client joins your world as "MCP Player", and the agent drives it around.

Point any MCP client at the game's port and it gets these tools.

`observe` returns a snapshot: where the bot is, where it's looking, the block it's aimed at, nearby players, recent chat, and the block in hand. Use it constantly, it's the bot's eyes.

`look` sets the bot's aim in degrees. `yaw` runs a full compass (0 south, 90 west) and `pitch` runs from -90 straight up to 90 straight down.

`move` holds movement keys like a person pressing them. Pass `forward`, `backward`, `left`, `right`, `up`, `down`, and `sprint`. Call it again to change direction, set them all false to stop. `{"forward": true, "sprint": true}` makes the bot run forward.

`jump` does a quick hop with no arguments. Pair it with `move` to clear walls.

`chat` sends a message broadcast to every player on the server.

`break_block` smashes the block the bot is aimed at. Aim with `look`, confirm with `observe`, then break.

`place_block` builds in front of the aimed-at block's face. Pass `type` as a real block name (`STONE`, `DIRT`, `GRASS`, `SAND`, `LOG`) or its index, and it goes right against what you're looking at.

That's enough for an agent to scout, farm, and build on its own. Point it at a corner, `move` over, `place_block`, done. The "MCP Player" client means you can watch the AI work from your own screen, or the AI watches you and copies.

## Talking to the bot in-game

Don't want a fancy MCP client? The bot also takes plain chat commands. `@bot`, `@mcp`, `/bot`, and `/mcp` all work, or type the verb straight in.

```
@bot go there
@bot come here
@bot follow me
@bot mine some stone
@bot mine that
@bot place stone
@bot place some stone there
@bot look at me
@bot look west
@bot jump
@bot stop
@bot help
```

The bot replies in chat when it's on it, and when it's stuck. Say `run` instead of `go` and it sprints. `mine` takes a material name or "that" for whatever you're aiming at, and `place` takes a material plus an optional "there". It walks over, does the job, and tells you when it's done.

## Building

Requires C++17, SFML 2.6, and CMake. MinGW on Windows, any normal toolchain elsewhere.

```
cmake -B build
cmake --build build -j
```

Two executables come out. `voxlord` is the client for creating, hosting, or joining. `voxlord-server` is the headless server for hosting without being in the game.

At configure time you can set `VIEW_DISTANCE` (how far chunks load, default 32 sections) and `LOADING_WORKERS_COUNT` (threads for chunk loading, default 4). `cmake -B build -DVIEW_DISTANCE=48` is how you's do that.

## Controls

Mouse look. `WASD` moves, `Space` jumps, `Shift` sprints. Scroll or the number row picks the hotbar block. Left click breaks, right click places. `T` opens chat, `Tab` toggles it. `Esc` frees the mouse and heads to the menu.

## Blurb

Minecraft at home, with a bot you can boss around. Light works, caves go deep, trees are everywhere, friends can hop in, and an AI can play it for you when you're lazy. Go build something stupid.