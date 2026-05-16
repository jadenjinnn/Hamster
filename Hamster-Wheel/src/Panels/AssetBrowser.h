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
};

#endif // UIPROTO_ASSET_BROWSER_H
