#include "PropertyEditor.h"
#include "Panels/ColliderEditor.h"
#include "../Panel.h"
#include "../Theme.h"
#include "../Components/Components.h"
#include "IconsFontAwesome6.h"

#include <Utils/AssetManager.h>
#include <Scripting/Scripting.h>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

static Panel g_PropPanel = {"Property Editor", "...##pe", false, true};

PropertyEditor::PropertyEditor(Hamster::EventDispatcher *dispatcher,
                               std::shared_ptr<Hamster::Scene> scene,
                               Hamster::AssetManager *assetManager,
                               ColliderEditor *colliderEditor)
    : m_Dispatcher(dispatcher), m_Scene(std::move(scene)),
      m_AssetManager(assetManager), m_ColliderEditor(colliderEditor) {
    m_ActiveSceneSub = m_Dispatcher->Subscribe(
        Hamster::ActiveSceneChanged,
        FORWARD_CALLBACK_FUNCTION(PropertyEditor::OnActiveSceneChanged,
                                  Hamster::ActiveSceneChangedEvent));
}

PropertyEditor::~PropertyEditor() {
    m_Dispatcher->Unsubscribe(Hamster::ActiveSceneChanged, m_ActiveSceneSub);
}

void PropertyEditor::OnActiveSceneChanged(Hamster::ActiveSceneChangedEvent &e) {
    m_Scene = e.GetActiveScene();
    m_SelectedEntity = Hamster::UUID::GetNil();
    m_Name = nullptr;
    m_Transform = nullptr;
    m_Sprite = nullptr;
    m_Rigidbody = nullptr;
    m_Animation = nullptr;
    m_Behaviour = nullptr;
    m_UIButton = nullptr;
    m_UIText = nullptr;
}

void PropertyEditor::SetSelectedEntity(Hamster::UUID uuid) {
    m_SelectedEntity = uuid;

    if (Hamster::UUID::IsNil(uuid) || !m_Scene) {
        m_Name = nullptr;
        m_Transform = nullptr;
        m_Sprite = nullptr;
        m_Rigidbody = nullptr;
        m_Animation = nullptr;
        m_Behaviour = nullptr;
        m_UIButton = nullptr;
        m_UIText = nullptr;
        return;
    }

    m_Name      = m_Scene->EntityHasComponent<Hamster::Name>(uuid)
                  ? &m_Scene->GetEntityComponent<Hamster::Name>(uuid) : nullptr;
    m_Transform = m_Scene->EntityHasComponent<Hamster::Transform>(uuid)
                  ? &m_Scene->GetEntityComponent<Hamster::Transform>(uuid) : nullptr;
    m_Sprite    = m_Scene->EntityHasComponent<Hamster::Sprite>(uuid)
                  ? &m_Scene->GetEntityComponent<Hamster::Sprite>(uuid) : nullptr;
    m_Rigidbody = m_Scene->EntityHasComponent<Hamster::Rigidbody>(uuid)
                  ? &m_Scene->GetEntityComponent<Hamster::Rigidbody>(uuid) : nullptr;
    m_Animation = m_Scene->EntityHasComponent<Hamster::Animation>(uuid)
                  ? &m_Scene->GetEntityComponent<Hamster::Animation>(uuid) : nullptr;
    m_Behaviour = m_Scene->EntityHasComponent<Hamster::Behaviour>(uuid)
                  ? &m_Scene->GetEntityComponent<Hamster::Behaviour>(uuid) : nullptr;
    m_UIButton  = m_Scene->EntityHasComponent<Hamster::UIButton>(uuid)
                  ? &m_Scene->GetEntityComponent<Hamster::UIButton>(uuid) : nullptr;
    m_UIText    = m_Scene->EntityHasComponent<Hamster::UIText>(uuid)
                  ? &m_Scene->GetEntityComponent<Hamster::UIText>(uuid) : nullptr;
}

void PropertyEditor::Render() {
    g_PropPanel.DrawHeader();
    g_PropPanel.BeginContent();

    // Lock the panel while simulation is running. Edits would be discarded
    // on stop anyway (simulation-snapshot reverts everything), so we make
    // that explicit by greying out the widgets — matches Unity. Live values
    // remain readable.
    const bool simRunning = m_Scene && !m_Scene->IsSceneSimulationPaused();
    ImGui::BeginDisabled(simRunning);

    if (Hamster::UUID::IsNil(m_SelectedEntity)) {
        ImGui::TextDisabled("No entity selected");
        ImGui::EndDisabled();
        g_PropPanel.EndContent();
        return;
    }

    // Entity name field
    if (m_Name) {
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - kScrollGap);
        ImGui::InputText("##EntityName", &m_Name->name);
        ImGui::PopItemWidth();
        ImGui::TextDisabled("%s", m_SelectedEntity.GetUUIDString().c_str());
        ImGui::Dummy({0, 4});
    }

    float avail = ImGui::GetContentRegionAvail().x - kScrollGap;
    float fieldW2 = (avail - 8.0f) * 0.5f;
    float fieldW3 = (avail - 8.0f * 2) / 3.0f;

    // ── Transform ──
    if (m_Transform) {
        SectionHeader(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT "  Transform");

        ImGui::Text("Position");
        AxisDotInput("X", "##PX", &m_Transform->position.x,
                     IM_COL32(220, 70, 70, 255), fieldW3);
        AxisDotInput("Y", "##PY", &m_Transform->position.y,
                     IM_COL32(70, 180, 100, 255), fieldW3);
        AxisDotInput("Z", "##PZ", &m_Transform->position.z,
                     IM_COL32(70, 130, 230, 255), fieldW3);
        ImGui::NewLine();
        ImGui::Dummy({0, 2});

        ImGui::Text("Scale");
        AxisDotInput("X", "##SX", &m_Transform->size.x,
                     IM_COL32(220, 70, 70, 255), fieldW2);
        AxisDotInput("Y", "##SY", &m_Transform->size.y,
                     IM_COL32(70, 180, 100, 255), fieldW2);
        ImGui::NewLine();
        ImGui::Dummy({0, 2});

        ImGui::Text("Rotation");
        ImGui::PushItemWidth(avail);
        ImGui::InputFloat("##Rot", &m_Transform->rotation, 0, 0, "%.2f\xC2\xB0");
        ImGui::PopItemWidth();

        SectionSeparator();
    }

    // ── Sprite ──
    if (m_Sprite) {
        SectionHeader(ICON_FA_IMAGE "  Sprite");

        // Resolve the sprite the same way the renderer does, so this preview
        // matches the viewport: when assetUUID is set it is the source of
        // truth (texture UUID → whole texture; sub-sprite UUID → parent
        // texture + UV sub-rect), otherwise fall back to the legacy texture
        // pointer at full UV.
        Hamster::Texture *previewTex = nullptr;
        glm::vec4 uv(0.0f, 0.0f, 1.0f, 1.0f);  // {u0, v0, w, h} normalised
        bool missing = false;
        std::string assetName = "None";
        int aw = 0, ah = 0;
        if (!Hamster::UUID::IsNil(m_Sprite->assetUUID)) {
            auto src = m_AssetManager->ResolveSpriteSource(m_Sprite->assetUUID);
            previewTex = src.texture;
            uv = src.uvRect;
            missing = src.missing;
            if (auto ss = m_AssetManager->GetSubSprite(m_Sprite->assetUUID)) {
                assetName = ss->name;
                aw = ss->pixelRect.z;
                ah = ss->pixelRect.w;
            } else if (!missing && src.texture) {
                // Use the already-resolved texture — do NOT call GetTexture()
                // here: it does m_Textures.at(uuid) and throws when the UUID
                // is a (now-deleted) sub-sprite rather than a texture.
                assetName = src.texture->GetName();
                aw = src.texture->GetWidth();
                ah = src.texture->GetHeight();
            }
        } else if (m_Sprite->texture) {
            previewTex = m_Sprite->texture.get();
            assetName = m_Sprite->texture->GetName();
            aw = m_Sprite->texture->GetWidth();
            ah = m_Sprite->texture->GetHeight();
        }

        float thumbSz = 64.0f;
        ImVec2 tp = ImGui::GetCursorScreenPos();
        ImDrawList *dl = ImGui::GetWindowDrawList();

        // Checkerboard background
        float cellSz = 8.0f;
        for (float y = 0; y < thumbSz; y += cellSz) {
            for (float x = 0; x < thumbSz; x += cellSz) {
                int ix = (int)(x / cellSz), iy = (int)(y / cellSz);
                ImU32 col = ((ix + iy) % 2 == 0) ? IM_COL32(60, 60, 65, 255)
                                                  : IM_COL32(40, 40, 45, 255);
                dl->AddRectFilled({tp.x + x, tp.y + y},
                                  {tp.x + std::min(x + cellSz, thumbSz),
                                   tp.y + std::min(y + cellSz, thumbSz)}, col);
            }
        }

        // Preview image — aspect-fit, UV-clipped to the resolved region.
        if (previewTex && previewTex->GetTextureId() != 0) {
            ImVec4 tint(m_Sprite->colour.r, m_Sprite->colour.g,
                        m_Sprite->colour.b, 1.0f);
            ImVec2 uv0{uv.x, uv.y};
            ImVec2 uv1{uv.x + uv.z, uv.y + uv.w};
            float rw = aw > 0 ? (float)aw : 1.0f;
            float rh = ah > 0 ? (float)ah : 1.0f;
            float scale = std::min(thumbSz / rw, thumbSz / rh);
            if (scale <= 0.0f) scale = 1.0f;
            float dw = rw * scale, dh = rh * scale;
            ImVec2 i0{tp.x + (thumbSz - dw) * 0.5f, tp.y + (thumbSz - dh) * 0.5f};
            ImVec2 i1{i0.x + dw, i0.y + dh};
            dl->AddImage(reinterpret_cast<ImTextureID>(
                             static_cast<intptr_t>(previewTex->GetTextureId())),
                         i0, i1, uv0, uv1,
                         ImGui::ColorConvertFloat4ToU32(tint));
        }

        // The thumbnail is a drop target for sub-sprites dragged from the
        // Asset Browser (HAMSTER_SUBSPRITE_UUIDS; the first UUID wins).
        ImGui::Dummy({thumbSz, thumbSz});
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload *p =
                    ImGui::AcceptDragDropPayload("HAMSTER_SUBSPRITE_UUIDS")) {
                const unsigned char *data =
                    static_cast<const unsigned char *>(p->Data);
                uint32_t count = 0;
                std::memcpy(&count, data, sizeof(count));
                if (count > 0) {
                    boost::uuids::uuid raw;
                    std::memcpy(&raw, data + sizeof(count), sizeof(raw));
                    m_Sprite->assetUUID = Hamster::UUID(raw);
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::SameLine();
        ImGui::BeginGroup();

        if (missing) {
            ImGui::TextColored({0.9f, 0.22f, 0.27f, 1.0f}, "MISSING");
            ImGui::TextDisabled("- x -");
        } else if (previewTex) {
            ImGui::TextDisabled("%s", assetName.c_str());
            ImGui::TextDisabled("%dx%d", aw, ah);
        } else {
            ImGui::TextDisabled("None");
            ImGui::TextDisabled("- x -");
        }

        ImGui::Text("Tint");
        ImGui::SameLine();
        float col[3] = {m_Sprite->colour.r, m_Sprite->colour.g, m_Sprite->colour.b};
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - kScrollGap);
        if (ImGui::ColorEdit3("##Tint", col, ImGuiColorEditFlags_NoLabel)) {
            m_Sprite->colour = glm::vec3(col[0], col[1], col[2]);
        }
        ImGui::PopItemWidth();
        ImGui::EndGroup();

        ImGui::Dummy({0, 8});
        if (HButton("Select Sprite", avail)) {
            ImGui::OpenPopup("Select Sprite");
        }
        if (HBeginStyledPopup("Select Sprite")) {
            // Sub-sprites first (named, more specific), then whole textures.
            // Both set assetUUID — the renderer's source of truth. Styled
            // items (HComboItem) keep this consistent with the panel's other
            // dropdowns (Add Component / Add Script).
            // PushID(uuid) keeps each item's ID unique while HComboItem shows
            // the clean name (HComboItem draws the label verbatim — it does
            // not strip a "##" suffix the way ImGui::Selectable does).
            for (const auto &[uuid, ss] : m_AssetManager->GetSubSpriteMap()) {
                if (!ss) continue;
                ImGui::PushID(ss->uuid.GetUUIDString().c_str());
                bool isSel =
                    m_Sprite->assetUUID.GetUUID() == ss->uuid.GetUUID();
                if (HComboItem(ss->name.c_str(), isSel)) {
                    m_Sprite->assetUUID = ss->uuid;
                }
                ImGui::PopID();
            }
            for (const auto &[uuid, texture] : m_AssetManager->GetTextureMap()) {
                ImGui::PushID(texture->GetUUID().GetUUIDString().c_str());
                bool isSel = m_Sprite->assetUUID.GetUUID() ==
                             texture->GetUUID().GetUUID();
                if (HComboItem(texture->GetName().c_str(), isSel)) {
                    m_Sprite->assetUUID = texture->GetUUID();
                }
                ImGui::PopID();
            }
            HEndStyledPopup();
        }

        SectionSeparator();
    }

    // ── Scripts ──
    if (m_Behaviour) {
        SectionHeader(ICON_FA_CODE "  Scripts");
        ImGui::Dummy({0, 2});

        Hamster::UUID removeScriptUUID = Hamster::UUID::GetNil();
        Hamster::UUID reassignFrom     = Hamster::UUID::GetNil();
        Hamster::UUID reassignTo       = Hamster::UUID::GetNil();

        for (auto &[uuid, script] : m_Behaviour->scripts) {
            Hamster::UUID mutableUUID = uuid;
            const bool missing = !script;

            std::string label;
            if (missing) {
                auto it = m_Behaviour->cachedNames.find(uuid);
                std::string cached =
                    (it != m_Behaviour->cachedNames.end() && !it->second.empty())
                        ? it->second
                        : "(unknown)";
                label = ICON_FA_TRIANGLE_EXCLAMATION "  MISSING: " + cached;
            } else {
                label = ICON_FA_FILE_CODE "  " + script->GetName();
            }
            const std::string fullId = label + "##" + mutableUUID.GetUUIDString();

            if (missing) {
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImVec4(0.95f, 0.4f, 0.4f, 1.0f));
            }
            HButton(fullId.c_str(), avail);
            if (missing) ImGui::PopStyleColor();

            if (ImGui::BeginPopupContextItem(
                    ("##sctx_" + mutableUUID.GetUUIDString()).c_str())) {
                if (missing) {
                    if (ImGui::BeginMenu("Reassign to")) {
                        for (const auto &[scrUuid, scrPtr] :
                             m_AssetManager->GetScriptMap()) {
                            if (m_Behaviour->scripts.count(scrUuid)) continue;
                            if (ImGui::MenuItem(scrPtr->GetName().c_str())) {
                                reassignFrom = uuid;
                                reassignTo = scrUuid;
                            }
                        }
                        ImGui::EndMenu();
                    }
                }
                if (ImGui::Selectable("Remove")) removeScriptUUID = uuid;
                ImGui::EndPopup();
            }
        }
        if (!Hamster::UUID::IsNil(removeScriptUUID)) {
            m_Behaviour->scripts.erase(removeScriptUUID);
            m_Behaviour->cachedNames.erase(removeScriptUUID);
        }
        if (!Hamster::UUID::IsNil(reassignFrom) &&
            !Hamster::UUID::IsNil(reassignTo)) {
            m_Behaviour->scripts.erase(reassignFrom);
            m_Behaviour->cachedNames.erase(reassignFrom);
            auto newScript = m_AssetManager->GetScript(reassignTo);
            m_Behaviour->scripts.emplace(reassignTo, newScript);
            if (newScript) {
                m_Behaviour->cachedNames[reassignTo] = newScript->GetName();
            }
        }

        ImGui::Dummy({0, 4});
        if (HButton("Add Script", avail)) {
            ImGui::OpenPopup("Add Script");
        }
        if (HBeginStyledPopup("Add Script")) {
            if (HComboItem(ICON_FA_PLUS "  New Script", false)) {
                Hamster::UUID newId = m_AssetManager->AddDefaultScript();
                auto newScript = m_AssetManager->GetScript(newId);
                m_Behaviour->scripts.emplace(newId, newScript);
                if (newScript) {
                    m_Behaviour->cachedNames[newId] = newScript->GetName();
                }
            }
            for (const auto &[uuid, script] : m_AssetManager->GetScriptMap()) {
                if (m_Behaviour->scripts.count(uuid) == 0) {
                    if (HComboItem(script->GetName().c_str(), false)) {
                        m_Behaviour->scripts.emplace(script->GetUUID(), script);
                        m_Behaviour->cachedNames[script->GetUUID()] =
                            script->GetName();
                    }
                }
            }
            HEndStyledPopup();
        }

        SectionSeparator();
    }

    // ── Rigidbody ──
    if (m_Rigidbody) {
        SectionHeader(ICON_FA_SHAPES "  Rigidbody");
        ImGui::Dummy({0, 2});

        const char *bodyTypes[] = {"Static", "Dynamic", "Kinematic"};
        int btIdx = static_cast<int>(m_Rigidbody->bodyType);
        HCombo("Body Type", "##bodytype", &btIdx, bodyTypes, 3);
        m_Rigidbody->bodyType = static_cast<Hamster::BodyType>(btIdx);

        const char *shapes[] = {"Box", "Circle"};
        int shapeIdx = static_cast<int>(m_Rigidbody->colliderShape);
        HCombo("Collider", "##collidershape", &shapeIdx, shapes, 2);
        m_Rigidbody->colliderShape = static_cast<Hamster::ColliderShape>(shapeIdx);

        HDragFloat("Density",       "##density",      &m_Rigidbody->density,      0.05f, 0.0f, 100.0f);
        HDragFloat("Friction",      "##friction",     &m_Rigidbody->friction,     0.01f, 0.0f, 1.0f);
        HDragFloat("Restitution",   "##restitution",  &m_Rigidbody->restitution,  0.01f, 0.0f, 1.0f);
        HDragFloat("Gravity Scale", "##gravityscale", &m_Rigidbody->gravityScale, 0.05f, -10.0f, 10.0f);

        ImGui::Dummy({0, 4});
        if (HButton(ICON_FA_VECTOR_SQUARE "  Edit Collider", avail)) {
            if (m_ColliderEditor) {
                m_ColliderEditor->Open(m_SelectedEntity, m_Scene);
            }
        }

        SectionSeparator();
    }

    // ── Animation ──
    if (m_Animation) {
        SectionHeader(ICON_FA_FILM "  Animation");
        ImGui::Dummy({0, 2});

        HCheckbox("Loop", "##animloop", &m_Animation->loop);

        ImGui::Text("Default");
        ImGui::SameLine(kLabelCol);
        const char *preview = m_Animation->defaultAnimation.empty()
                                  ? "None"
                                  : m_Animation->defaultAnimation.c_str();
        if (HBeginCombo("##defaultanim", preview)) {
            if (HComboItem("None", m_Animation->defaultAnimation.empty())) {
                m_Animation->defaultAnimation.clear();
            }
            for (auto &[name, uuid] : m_Animation->animations) {
                if (HComboItem(name.c_str(), name == m_Animation->defaultAnimation)) {
                    m_Animation->defaultAnimation = name;
                }
            }
            HEndCombo();
        }

        ImGui::Dummy({0, 4});
        ImGui::Text("Animations");

        std::string removeKey;
        for (auto &[name, uuid] : m_Animation->animations) {
            std::string id = ICON_FA_FILM "  " + name + "##" + uuid.GetUUIDString();
            HButton(id.c_str(), avail);
            if (ImGui::BeginPopupContextItem(("##actx_" + name).c_str())) {
                if (ImGui::Selectable("Remove")) removeKey = name;
                ImGui::EndPopup();
            }
        }
        if (!removeKey.empty()) {
            m_Animation->animations.erase(removeKey);
            if (m_Animation->defaultAnimation == removeKey) {
                m_Animation->defaultAnimation.clear();
            }
        }

        ImGui::Dummy({0, 4});
        if (HButton(ICON_FA_PLUS "  Add Animation", avail)) {
            ImGui::OpenPopup("Add Animation");
        }
        if (ImGui::BeginPopup("Add Animation")) {
            for (auto &[uuid, animData] : m_AssetManager->GetAnimationMap()) {
                bool alreadyAdded = false;
                for (auto &[n, u] : m_Animation->animations) {
                    if (u.GetUUID() == uuid.GetUUID()) { alreadyAdded = true; break; }
                }
                if (!alreadyAdded) {
                    if (ImGui::Selectable(animData->name.c_str())) {
                        m_Animation->animations[animData->name] = uuid;
                    }
                }
            }
            ImGui::EndPopup();
        }

        SectionSeparator();
    }

    // ── UI Button ──
    if (m_UIButton) {
        SectionHeader(ICON_FA_SQUARE "  UI Button");
        ImGui::Dummy({0, 2});

        const char *anchors[] = {
            "Top Left", "Top Centre", "Top Right",
            "Middle Left", "Centre", "Middle Right",
            "Bottom Left", "Bottom Centre", "Bottom Right",
        };
        int anchorIdx = static_cast<int>(m_UIButton->anchor);
        HCombo("Anchor", "##uibtn_anchor", &anchorIdx, anchors, 9);
        m_UIButton->anchor = static_cast<Hamster::UIAnchor>(anchorIdx);

        ImGui::Text("Offset (px from anchor, +moves inward)");
        AxisDotInput("X", "##uibtn_offx", &m_UIButton->offset.x,
                     IM_COL32(220, 70, 70, 255), fieldW2);
        AxisDotInput("Y", "##uibtn_offy", &m_UIButton->offset.y,
                     IM_COL32(70, 180, 100, 255), fieldW2);
        ImGui::NewLine();
        ImGui::Dummy({0, 2});

        HCheckbox("Auto Size", "##uibtn_auto", &m_UIButton->autoSize);
        if (!m_UIButton->autoSize) {
            ImGui::Text("Size (px)");
            AxisDotInput("W", "##uibtn_w", &m_UIButton->size.x,
                         IM_COL32(220, 70, 70, 255), fieldW2);
            AxisDotInput("H", "##uibtn_h", &m_UIButton->size.y,
                         IM_COL32(70, 180, 100, 255), fieldW2);
            ImGui::NewLine();
        } else {
            HDragFloat("Padding", "##uibtn_pad", &m_UIButton->padding, 0.5f, 0.0f, 64.0f);
        }
        ImGui::Dummy({0, 2});

        ImGui::Text("Background");
        ImGui::SameLine();
        float bg[4] = {m_UIButton->bgColour.r, m_UIButton->bgColour.g,
                       m_UIButton->bgColour.b, m_UIButton->bgColour.a};
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - kScrollGap);
        // AlphaBar surfaces the alpha channel as a draggable bar — drag it to 0
        // for a fully transparent background (image-only buttons).
        if (ImGui::ColorEdit4("##uibtn_bg", bg,
                              ImGuiColorEditFlags_NoLabel |
                                  ImGuiColorEditFlags_AlphaBar)) {
            m_UIButton->bgColour = glm::vec4(bg[0], bg[1], bg[2], bg[3]);
        }
        ImGui::PopItemWidth();

        ImGui::Dummy({0, 4});

        ImGui::Text("Label");
        ImGui::PushItemWidth(avail);
        ImGui::InputText("##uibtn_label", &m_UIButton->label);
        ImGui::PopItemWidth();

        ImGui::Text("Text Colour");
        ImGui::SameLine();
        float tc[4] = {m_UIButton->textColour.r, m_UIButton->textColour.g,
                       m_UIButton->textColour.b, m_UIButton->textColour.a};
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - kScrollGap);
        if (ImGui::ColorEdit4("##uibtn_tc", tc, ImGuiColorEditFlags_NoLabel)) {
            m_UIButton->textColour = glm::vec4(tc[0], tc[1], tc[2], tc[3]);
        }
        ImGui::PopItemWidth();

        HDragFloat("Font Size", "##uibtn_fs", &m_UIButton->fontSize,
                   0.5f, 4.0f, 200.0f);

        const char *aligns[] = {"Left", "Centre", "Right"};
        int alignIdx = static_cast<int>(m_UIButton->textAlign);
        HCombo("Align", "##uibtn_align", &alignIdx, aligns, 3);
        m_UIButton->textAlign = static_cast<Hamster::UITextAlign>(alignIdx);

        HCheckbox("Bold", "##uibtn_bold", &m_UIButton->bold);

        ImGui::Dummy({0, 6});
        // Optional background image. Mirrors the Sprite "Select Sprite" picker:
        // sub-sprites first (named), then whole textures; both set imageUUID.
        ImGui::Text("Image");
        if (!Hamster::UUID::IsNil(m_UIButton->imageUUID)) {
            std::string imgName = "(missing)";
            if (auto ss = m_AssetManager->GetSubSprite(m_UIButton->imageUUID)) {
                imgName = ss->name;
            } else {
                const auto &texMap = m_AssetManager->GetTextureMap();
                auto it = texMap.find(m_UIButton->imageUUID);
                if (it != texMap.end() && it->second)
                    imgName = it->second->GetName();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s", imgName.c_str());
        }
        if (HButton("Select Image", avail)) {
            ImGui::OpenPopup("Select Image");
        }
        if (HBeginStyledPopup("Select Image")) {
            if (HComboItem("(none)", Hamster::UUID::IsNil(m_UIButton->imageUUID)))
                m_UIButton->imageUUID = Hamster::UUID::GetNil();
            for (const auto &[uuid, ss] : m_AssetManager->GetSubSpriteMap()) {
                if (!ss) continue;
                ImGui::PushID(ss->uuid.GetUUIDString().c_str());
                bool isSel =
                    m_UIButton->imageUUID.GetUUID() == ss->uuid.GetUUID();
                if (HComboItem(ss->name.c_str(), isSel))
                    m_UIButton->imageUUID = ss->uuid;
                ImGui::PopID();
            }
            for (const auto &[uuid, texture] : m_AssetManager->GetTextureMap()) {
                ImGui::PushID(texture->GetUUID().GetUUIDString().c_str());
                bool isSel = m_UIButton->imageUUID.GetUUID() ==
                             texture->GetUUID().GetUUID();
                if (HComboItem(texture->GetName().c_str(), isSel))
                    m_UIButton->imageUUID = texture->GetUUID();
                ImGui::PopID();
            }
            HEndStyledPopup();
        }

        SectionSeparator();
    }

    // ── UI Text ──
    if (m_UIText) {
        SectionHeader(ICON_FA_FONT "  UI Text");
        ImGui::Dummy({0, 2});

        const char *anchors[] = {
            "Top Left", "Top Centre", "Top Right",
            "Middle Left", "Centre", "Middle Right",
            "Bottom Left", "Bottom Centre", "Bottom Right",
        };
        int anchorIdx = static_cast<int>(m_UIText->anchor);
        HCombo("Anchor", "##uitxt_anchor", &anchorIdx, anchors, 9);
        m_UIText->anchor = static_cast<Hamster::UIAnchor>(anchorIdx);

        ImGui::Text("Offset");
        AxisDotInput("X", "##uitxt_offx", &m_UIText->offset.x,
                     IM_COL32(220, 70, 70, 255), fieldW2);
        AxisDotInput("Y", "##uitxt_offy", &m_UIText->offset.y,
                     IM_COL32(70, 180, 100, 255), fieldW2);
        ImGui::NewLine();
        ImGui::Dummy({0, 2});

        ImGui::Text("Text");
        ImGui::PushItemWidth(avail);
        ImGui::InputText("##uitxt_text", &m_UIText->text);
        ImGui::PopItemWidth();

        ImGui::Text("Text Colour");
        ImGui::SameLine();
        float ttc[4] = {m_UIText->textColour.r, m_UIText->textColour.g,
                        m_UIText->textColour.b, m_UIText->textColour.a};
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - kScrollGap);
        if (ImGui::ColorEdit4("##uitxt_tc", ttc, ImGuiColorEditFlags_NoLabel)) {
            m_UIText->textColour = glm::vec4(ttc[0], ttc[1], ttc[2], ttc[3]);
        }
        ImGui::PopItemWidth();

        HDragFloat("Font Size", "##uitxt_fs", &m_UIText->fontSize,
                   0.5f, 4.0f, 200.0f);
        HDragFloat("Wrap Width", "##uitxt_wrap", &m_UIText->wrapWidth,
                   1.0f, 0.0f, 4096.0f);

        HCheckbox("Bold", "##uitxt_bold", &m_UIText->bold);

        SectionSeparator();
    }

    // ── Add Component ──
    {
        const char *label = ICON_FA_PLUS "  Add Component";
        ImVec2 textSz = ImGui::CalcTextSize(label);
        float padX = 14.0f, padY = 6.0f;
        float btnW = textSz.x + padX * 2;
        float btnH = textSz.y + padY * 2;

        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - btnW) * 0.5f);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##addcomp", {btnW, btnH});
        bool hovered = ImGui::IsItemHovered();
        bool held    = ImGui::IsItemActive();
        bool clicked = ImGui::IsItemClicked();

        ImDrawList *dl = ImGui::GetWindowDrawList();
        if (hovered || held) {
            ImU32 bg = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive
                                                : ImGuiCol_ButtonHovered);
            dl->AddRectFilled(pos, {pos.x + btnW, pos.y + btnH}, bg, 6.0f);
        }
        dl->AddText({pos.x + padX, pos.y + padY},
                    ImGui::ColorConvertFloat4ToU32(kAccent), label);

        if (clicked) {
            ImGui::OpenPopup("Add Component");
        }

        if (HBeginStyledPopup("Add Component")) {
            if (!m_Sprite && HComboItem(ICON_FA_IMAGE "  Sprite", false)) {
                m_Scene->AddEntityComponent<Hamster::Sprite>(m_SelectedEntity);
                m_Sprite = &m_Scene->GetEntityComponent<Hamster::Sprite>(m_SelectedEntity);
            }
            if (!m_Behaviour && HComboItem(ICON_FA_CODE "  Script", false)) {
                m_Scene->AddEntityComponent<Hamster::Behaviour>(m_SelectedEntity);
                m_Behaviour = &m_Scene->GetEntityComponent<Hamster::Behaviour>(m_SelectedEntity);
            }
            if (!m_Rigidbody && HComboItem(ICON_FA_SHAPES "  Rigidbody", false)) {
                m_Scene->AddEntityComponent<Hamster::Rigidbody>(m_SelectedEntity);
                m_Rigidbody = &m_Scene->GetEntityComponent<Hamster::Rigidbody>(m_SelectedEntity);
            }
            if (!m_Animation && HComboItem(ICON_FA_FILM "  Animation", false)) {
                m_Scene->AddEntityComponent<Hamster::Animation>(m_SelectedEntity);
                m_Animation = &m_Scene->GetEntityComponent<Hamster::Animation>(m_SelectedEntity);
            }
            if (!m_UIButton && HComboItem(ICON_FA_SQUARE "  UI Button", false)) {
                m_Scene->AddEntityComponent<Hamster::UIButton>(m_SelectedEntity);
                m_UIButton = &m_Scene->GetEntityComponent<Hamster::UIButton>(m_SelectedEntity);
            }
            if (!m_UIText && HComboItem(ICON_FA_FONT "  UI Text", false)) {
                m_Scene->AddEntityComponent<Hamster::UIText>(m_SelectedEntity);
                m_UIText = &m_Scene->GetEntityComponent<Hamster::UIText>(m_SelectedEntity);
            }
            HEndStyledPopup();
        }
    }

    ImGui::Dummy({0, 16});
    ImGui::EndDisabled();
    g_PropPanel.EndContent();
}
