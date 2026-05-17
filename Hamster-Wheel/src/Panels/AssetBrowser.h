#ifndef UIPROTO_ASSET_BROWSER_H
#define UIPROTO_ASSET_BROWSER_H

#include <Hamster.h>
#include <Core/UUID.h>
#include <memory>

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

    // Rename state — m_RenameUUID == nil means no rename in progress.
    Hamster::UUID m_RenameUUID = Hamster::UUID::GetNil();
    char          m_RenameBuffer[128] = {0};
    bool          m_OpenRenamePopup = false; // request the popup on next frame
    bool          m_RenameCollision = false; // shown inline in the modal
};

#endif // UIPROTO_ASSET_BROWSER_H
