//
// Created by Jaden on 01/09/2024.
//

#ifndef PROJECTHUBLAYER_H
#define PROJECTHUBLAYER_H

#include <Hamster.h>

#include "EditorLayer.h"
#include "Panels/ProjectSelector.h"


class ProjectHubLayer : public Hamster::Layer {
public:
    explicit ProjectHubLayer(Hamster::EventDispatcher *dispatcher)
        : m_Dispatcher(dispatcher),
          m_ProjectSelector(std::make_unique<ProjectSelector>(dispatcher)),
          m_ProjectCreator(std::make_unique<ProjectCreator>(dispatcher)) {
    };

    void OnAttach() override;

    void OnImGuiUpdate() override;

private:
    Hamster::EventDispatcher *m_Dispatcher;
    std::unique_ptr<ProjectSelector> m_ProjectSelector;
    std::unique_ptr<ProjectCreator> m_ProjectCreator;
    EditorLayer *m_EditorLayer = nullptr;
};


#endif //PROJECTHUBLAYER_H
