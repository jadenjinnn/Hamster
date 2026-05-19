#ifndef UIPROTO_ASSET_BROWSER_H
#define UIPROTO_ASSET_BROWSER_H

#include <Hamster.h>
#include <Core/UUID.h>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>

namespace Hamster {
class AssetManager;
}

class AssetBrowser {
public:
    AssetBrowser(Hamster::EventDispatcher *dispatcher,
                 std::shared_ptr<Hamster::Scene> scene,
                 Hamster::AssetManager *assetManager);
    ~AssetBrowser();

    void Render();

    void OnActiveSceneChanged(Hamster::ActiveSceneChangedEvent &e);

private:
    Hamster::EventDispatcher *m_Dispatcher;
    Hamster::SubscriptionHandle m_ActiveSceneSub = 0;
    std::shared_ptr<Hamster::Scene> m_Scene;
    Hamster::AssetManager *m_AssetManager;

    // Inline rename — set on new-script create and on right-click "Rename".
    // The card's label becomes an auto-focused InputText; commit on Enter /
    // click-away, Escape cancels. Extension is preserved verbatim (we only
    // edit the stem) so .py vs .png is recovered on commit.
    Hamster::UUID m_InlineRenameUUID = Hamster::UUID::GetNil();
    char          m_InlineRenameBuf[128] = {0};
    std::string   m_InlineRenameExt;
    bool          m_InlineRenameFocus = false;

    // Current folder for the script section, relative to the project root.
    // Empty string = project root. Single-level breadcrumb in v1; deeper
    // tree view is future work per the spec.
    std::filesystem::path m_CurrentFolder;

    // Spritesheet UI state.
    // Texture UUIDs whose sub-sprite mini-card grid is currently expanded.
    std::unordered_set<Hamster::UUID> m_ExpandedSheets;
    // Multi-select state — sub-sprite UUIDs currently selected for drag.
    // Cleared on background click, mutated by Ctrl-click.
    std::unordered_set<Hamster::UUID> m_SelectedSubSprites;
};

#endif // UIPROTO_ASSET_BROWSER_H
