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

private:
    int m_Tab = 0;
    std::unique_ptr<AssetBrowser> m_AssetBrowser;
    std::unique_ptr<AnimationPanel> m_AnimationPanel;
    std::unique_ptr<Console> m_Console;
};

#endif // UIPROTO_BOTTOM_PANEL_H
