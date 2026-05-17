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
#include "Utils/MetaFile.h"

namespace Hamster {
    std::filesystem::path Scripting::GenerateDefaultScript(UUID *uuidVal) {
        UUID scriptUUID;

        // Pick a human-friendly filename, bumping a numeric suffix until the
        // path is free. Identity now lives in the sidecar — no UUID baked into
        // the filename. Sidecar uniqueness is implied by filename uniqueness.
        const std::filesystem::path projectDir =
                Project::GetCurrentProject()->GetConfig().ProjectDirectory;
        std::filesystem::path scriptPath = projectDir / "Untitled_Script.py";
        for (int suffix = 1;
             std::filesystem::exists(scriptPath) ||
             std::filesystem::exists(MetaFile::SidecarPath(scriptPath));
             ++suffix) {
            scriptPath = projectDir /
                         ("Untitled_Script_" + std::to_string(suffix) + ".py");
        }

        std::ofstream scriptOut(scriptPath);

        std::string defaultContent = "import Hamster\n\n"
                "class test(Hamster.HamsterBehaviour):\n"
                "    def on_update(self, delta_time):\n"
                "        pass\n";

        scriptOut << defaultContent;

        scriptOut.close();

        MetaFile::Write(scriptPath, scriptUUID);

        if (uuidVal != nullptr) {
            *uuidVal = scriptUUID;
        }

        return scriptPath;
    }

    void Scripting::InitInterpreter(EventDispatcher *dispatcher) {
        if (!m_InterpreterInitialised) {
            pybind11::initialize_interpreter();

            m_InterpreterInitialised = true;

            dispatcher->Subscribe(
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
