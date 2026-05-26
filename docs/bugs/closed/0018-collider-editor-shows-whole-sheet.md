# Bug 0018: Collider Editor preview shows the whole spritesheet, not the sub-sprite

> Status: **fixed**
> Severity: **Low**
> Tier: **1**
> Logged: 2026-05-26
> Found while: building the Flappy Bird demo game

---

## Symptom

For an entity whose Sprite is a spritesheet sub-sprite, the Collider Editor draws the **entire sheet** behind the collider handles instead of just the entity's sub-sprite region.

## Suspected location

`Hamster-Wheel/src/Panels/ColliderEditor.cpp`, sprite preview draw.

## Root cause

The preview drew `sprite->texture` (the parent/whole texture) with full UVs `{0,0}→{1,1}`, ignoring `Sprite::assetUUID`. Sub-sprites are stored as a parent texture + a UV sub-rect, resolved via `AssetManager::ResolveSpriteSource`; the Collider Editor never resolved it (the same gap the PropertyEditor had at spritesheet stage 8, which was fixed there but not here).

## Fix

`ColliderEditor.cpp` now resolves the Sprite via `m_Scene->GetApp()->GetAssetManager()->ResolveSpriteSource(sprite->assetUUID)` and draws the returned UV sub-rect (falling back to the legacy `sprite->texture` with full UVs when `assetUUID` is nil) — mirroring `Scene::OnRender` and the PropertyEditor.

## Verification

- Debug build compiles; editor: open the Collider Editor on a sub-sprite entity (the Flappy Bird bird) → shows only the bird, not the sheet. Confirmed by author.

---

## Related

- Same class as the PropertyEditor sub-sprite-resolution fix (spritesheet stage 8, decisions.md 2026-05-25).
