#ifndef UIPROTO_BOTTOM_PANEL_H
#define UIPROTO_BOTTOM_PANEL_H

#include <Hamster.h>
#include <memory>

#include "AssetBrowser.h"
#include "AnimationPanel.h"
#include "Console.h"

namespace Hamster {
class AssetManager;
}

class SpritesheetEditor;

class BottomPanel {
public:
    BottomPanel(Hamster::EventDispatcher *dispatcher,
                std::shared_ptr<Hamster::Scene> scene,
                Hamster::AssetManager *assetManager,
                SpritesheetEditor *spritesheetEditor);

    void Render();

    // Switch to the Console tab — called when the scene simulation starts so
    // script logs/errors are immediately visible. (The panel is focused via
    // ImGui::SetNextWindowFocus() in EditorLayer, which is the reliable path.)
    void ShowConsole() { m_Tab = 2; }

private:
    int m_Tab = 0;
    std::unique_ptr<AssetBrowser> m_AssetBrowser;
    std::unique_ptr<AnimationPanel> m_AnimationPanel;
    std::unique_ptr<Console> m_Console;
};

#endif // UIPROTO_BOTTOM_PANEL_H
