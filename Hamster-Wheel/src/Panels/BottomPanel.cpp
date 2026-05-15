#include "BottomPanel.h"
#include "../Panel.h"
#include "../Theme.h"

#include <imgui.h>

static Panel g_BottomPanel = {"", "...##bp", false, true};

BottomPanel::BottomPanel(Hamster::EventDispatcher *dispatcher,
                         std::shared_ptr<Hamster::Scene> scene,
                         Hamster::AssetManager *assetManager) {
    m_AssetBrowser   = std::make_unique<AssetBrowser>(dispatcher, scene, assetManager);
    m_AnimationPanel = std::make_unique<AnimationPanel>(dispatcher, scene, assetManager);
    m_Console        = std::make_unique<Console>(dispatcher, scene);
}

void BottomPanel::Render() {
    const char *tabs[] = {"Asset Browser", "Animation", "Console"};
    g_BottomPanel.DrawTabbedHeader(tabs, 3, &m_Tab);
    g_BottomPanel.BeginContent();

    if (m_Tab == 0)      m_AssetBrowser->Render();
    else if (m_Tab == 1) m_AnimationPanel->Render();
    else if (m_Tab == 2) m_Console->Render();

    g_BottomPanel.EndContent();
}
