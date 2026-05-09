//
// Created by Jaden on 27/08/2024.
//

#ifndef STARTPAUSEMODAL_H
#define STARTPAUSEMODAL_H

#include <Hamster.h>
#include <Gui/Panel.h>

#include <utility>


class StartPauseModal : public Hamster::Panel {
public:
    StartPauseModal(Hamster::EventDispatcher *dispatcher,
                    std::shared_ptr<Hamster::Scene> scene) : Hamster::Panel(dispatcher, scene, true) {
    };

    void Render() override;

private:
};


#endif //STARTPAUSEMODAL_H
