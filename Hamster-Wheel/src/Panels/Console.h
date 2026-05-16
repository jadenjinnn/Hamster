#ifndef UIPROTO_CONSOLE_H
#define UIPROTO_CONSOLE_H

#include <Hamster.h>
#include <Core/Log.h>
#include <memory>

class Console {
public:
    Console(Hamster::EventDispatcher *dispatcher,
            std::shared_ptr<Hamster::Scene> scene);
    ~Console();

    void Render();
    void OnActiveSceneChanged(Hamster::ActiveSceneChangedEvent &e);

private:
    Hamster::EventDispatcher *m_Dispatcher;
    Hamster::SubscriptionHandle m_ActiveSceneSub = 0;
    std::shared_ptr<Hamster::Scene> m_Scene;
    std::shared_ptr<Hamster::Logger> m_ClientLogger;
};

#endif // UIPROTO_CONSOLE_H
