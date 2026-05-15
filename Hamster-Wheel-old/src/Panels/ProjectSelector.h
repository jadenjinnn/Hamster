//
// Created by Jaden on 01/09/2024.
//

#ifndef PROJECTSELECTOR_H
#define PROJECTSELECTOR_H

#include <Hamster.h>
#include <Gui/Panel.h>
#include <Core/Project.h>

// #include <windows.h>
// #include <shlobj.h>


class ProjectSelector : public Hamster::Panel {
public:
    explicit ProjectSelector(Hamster::EventDispatcher *dispatcher) : Hamster::Panel(dispatcher) {
    };

    ProjectSelector(Hamster::EventDispatcher *dispatcher,
                    std::shared_ptr<Hamster::Scene> scene) : Hamster::Panel(dispatcher, scene, false) {
    };

    void Render() override;

private:
    // std::string OpenWindowsFileDialog(HWND owner);

    std::filesystem::path m_ProjectFilePath;

    bool m_NoPathSelected = false;
    bool m_WrongFileExtension = false;
};


#endif //PROJECTSELECTOR_H
