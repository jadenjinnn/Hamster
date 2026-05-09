//
// Created by jaden on 07/09/24.
//

#include "HamsterPCH.h"

#include "Scripting.h"

#include "Core/Application.h"
#include "Core/Components.h"
#include <entt/entity/entity.hpp>
#include <pybind11/embed.h>
#include <pybind11/eval.h>
#include <pybind11/pybind11.h>

#include "Core/Project.h"
#include "Scripting/HamsterScript.h"
#include "Utils/AssetManager.h"

namespace Hamster {
    std::filesystem::path Scripting::GenerateDefaultScript(UUID *uuidVal) {
        UUID scriptUUID;

        std::string fileName = "Untitled_Script_" + scriptUUID.GetUUIDString();

        std::filesystem::path scriptPath =
                Project::GetCurrentProject()->GetConfig().ProjectDirectory /
                (fileName + ".py");

        // Init();

        std::ofstream scriptOut(scriptPath);

        std::string defaultContent = "import Hamster\n\n"
                "class test(Hamster.HamsterBehaviour):\n"
                "    def on_update(self, delta_time):\n"
                "        pass\n";

        scriptOut << defaultContent;

        scriptOut.close();

        if (uuidVal != nullptr) {
            *uuidVal = scriptUUID;
        }

        return scriptPath;
    }

    void Scripting::InitInterpreter() {
        if (!m_InterpreterInitialised) {
            pybind11::initialize_interpreter();

            m_InterpreterInitialised = true;

            Application::GetApplicationInstance().GetEventDispatcher()->Subscribe(
                ProjectOpened, FORWARD_STATIC_CALLBACK_FUNCTION(Scripting::AddPathToPy,
                                                                ProjectOpenedEvent));
        }
    }

    void Scripting::AddPathToPy(ProjectOpenedEvent &e) {
        pybind11::module_ sys = pybind11::module::import("sys");

        pybind11::list path = sys.attr("path");

        path.append(e.GetPath().string());
    }
} // namespace Hamster
