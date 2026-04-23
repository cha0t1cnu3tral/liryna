# Servival Mind Spelling Hear Inventory

This document describes the new survival inventory ("Servival Mind Spelling Hear Inventory") and shared hotbar behavior.

## Survival Inventory Screen

- Screen name: `Survival inventory`
- Open/close key in world: `E`
- Navigation between categories: `Tab` and `Shift+Tab`
- Category layout uses container + grid widgets (`UI_CONTAINER_GRID`)
- Categories are shown even when empty (each empty category exposes an `Empty` entry by default)

## Item Selection and Hotbar Assignment

- Focus an item in creative or survival inventory.
- Press `Enter` to select that item for hotbar assignment.
- Press number key `1` through `9` to assign the selected item into that hotbar slot.
- In the world screen, pressing `1` through `9` selects that hotbar slot.

The hotbar system is shared by all game modes through `Inventory` (`INVENTORY_HOTBAR_SLOT_COUNT`), so future modes can reuse the same flow.

## Survival Pickup Controls

Pickup controls in survival world:

- `]` (right bracket)
- Right mouse click

The pickup action checks tiles around the player and can currently pick up furniture/tools (category `Furniture`).

## Pickup Restrictions (Current)

Not all tiles can be picked up directly.

- Furniture/tools: pickup allowed now
- Trees: blocked for now (tool-based harvesting planned)
- Rocks: blocked for now (tool-based harvesting planned)
- Ground/terrain pieces: blocked for now (tool-based harvesting planned)

When a blocked tile is targeted, the game announces that tool-based pickup is not implemented yet.
