//
// Created by Jaden on 30/08/2024.
//

#ifndef MENUBAR_H
#define MENUBAR_H
#include <memory>

#include "ProjectCreator.h"
#include "ProjectSelector.h"
#include "Core/Scene.h"
#include "Gui/Panel.h"


class MenuBar : public Hamster::Panel {
public:
    MenuBar(Hamster::EventDispatcher *dispatcher,
            std::shared_ptr<Hamster::Scene> scene) : Hamster::Panel(dispatcher, scene, true) {
        m_ProjectSelector = std::make_unique<ProjectSelector>(dispatcher, scene);
        m_ProjectCreator = std::make_unique<ProjectCreator>(dispatcher, scene);
    };

    void Render();

private:
    std::shared_ptr<Hamster::Scene> m_Scene;
    std::unique_ptr<ProjectCreator> m_ProjectCreator;
    std::unique_ptr<ProjectSelector> m_ProjectSelector;
};


#endif //MENUBAR_H
