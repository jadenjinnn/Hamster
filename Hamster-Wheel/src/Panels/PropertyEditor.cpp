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
#include <cstdio>

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
}

void PropertyEditor::Render() {
    g_PropPanel.DrawHeader();
    g_PropPanel.BeginContent();

    if (Hamster::UUID::IsNil(m_SelectedEntity)) {
        ImGui::TextDisabled("No entity selected");
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

        // Sprite texture (or placeholder)
        if (m_Sprite->texture && m_Sprite->texture->GetTextureId() != 0) {
            ImVec4 tint(m_Sprite->colour.r, m_Sprite->colour.g, m_Sprite->colour.b, 1.0f);
            dl->AddImage(
                reinterpret_cast<ImTextureID>(
                    static_cast<intptr_t>(m_Sprite->texture->GetTextureId())),
                tp, {tp.x + thumbSz, tp.y + thumbSz},
                {0, 0}, {1, 1}, ImGui::ColorConvertFloat4ToU32(tint));
        }

        ImGui::Dummy({thumbSz, thumbSz});
        ImGui::SameLine();
        ImGui::BeginGroup();

        if (m_Sprite->texture) {
            ImGui::TextDisabled("%s", m_Sprite->texture->GetName().c_str());
            ImGui::TextDisabled("%dx%d",
                                m_Sprite->texture->GetWidth(),
                                m_Sprite->texture->GetHeight());
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
        if (ImGui::BeginPopup("Select Sprite")) {
            for (const auto &[uuid, texture] : m_AssetManager->GetTextureMap()) {
                std::string id = texture->GetName() + "##" +
                                 texture->GetUUID().GetUUIDString();
                if (ImGui::Selectable(id.c_str())) {
                    m_Sprite->texture = texture;
                }
            }
            ImGui::EndPopup();
        }

        SectionSeparator();
    }

    // ── Scripts ──
    if (m_Behaviour) {
        SectionHeader(ICON_FA_CODE "  Scripts");
        ImGui::Dummy({0, 2});

        Hamster::UUID removeScriptUUID = Hamster::UUID::GetNil();
        for (auto &[uuid, script] : m_Behaviour->scripts) {
            Hamster::UUID mutableUUID = uuid;
            std::string id = ICON_FA_FILE_CODE "  " + script->GetName() +
                             "##" + mutableUUID.GetUUIDString();
            HButton(id.c_str(), avail);
            if (ImGui::BeginPopupContextItem(("##sctx_" + mutableUUID.GetUUIDString()).c_str())) {
                if (ImGui::Selectable("Remove")) removeScriptUUID = uuid;
                ImGui::EndPopup();
            }
        }
        if (!Hamster::UUID::IsNil(removeScriptUUID)) {
            m_Behaviour->scripts.erase(removeScriptUUID);
        }

        ImGui::Dummy({0, 4});
        if (HButton("Add Script", avail)) {
            ImGui::OpenPopup("Add Script");
        }
        if (ImGui::BeginPopup("Add Script")) {
            if (ImGui::Selectable(ICON_FA_PLUS "  New Script")) {
                Hamster::UUID newId = m_AssetManager->AddDefaultScript();
                auto newScript = m_AssetManager->GetScript(newId);
                m_Behaviour->scripts.emplace(newId, newScript);
            }
            if (m_AssetManager->GetScriptCount() > 0) ImGui::Separator();
            for (const auto &[uuid, script] : m_AssetManager->GetScriptMap()) {
                if (m_Behaviour->scripts.count(uuid) == 0) {
                    if (ImGui::Selectable(script->GetName().c_str())) {
                        m_Behaviour->scripts.emplace(script->GetUUID(), script);
                    }
                }
            }
            ImGui::EndPopup();
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
            HEndStyledPopup();
        }
    }

    ImGui::Dummy({0, 16});
    g_PropPanel.EndContent();
}
