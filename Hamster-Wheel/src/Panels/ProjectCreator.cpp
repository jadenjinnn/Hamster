//
// Created by Jaden on 31/08/2024.
//

#include "ProjectCreator.h"

#include <filesystem>
#include <imgui.h>

#include "tinyfiledialogs.h"

// #include <GLFW/glfw3.h>
// #include <GLFW/glfw3native.h>

void ProjectCreator::Render() {
  if (!ImGui::Begin("Project Creator", &m_WindowOpen)) {
    ImGui::End();
    return;
  }

  ImGui::Text("Project Name");
  ImGui::SameLine();
  ImGui::InputText("##textInput#", m_ProjectNameInput,
                   IM_ARRAYSIZE(m_ProjectNameInput));

  if (ImGui::IsItemActivated()) {
    m_DirectoryExists = false;
  }

  ImGui::Text("Project Directory");
  ImGui::SameLine();

  if (ImGui::Button("Select Directory")) {
    m_NoDirectorySelected = false;

    // HWND owner =
    // glfwGetWin32Window(Hamster::Application::GetApplicationInstance().GetWindow());
    //
    // m_ProjectConfig.ProjectDirectory = OpenWindowsFileDialog(owner);

    const char *selectedDirectory =
        tinyfd_selectFolderDialog("Select directory for project", "");

    if (selectedDirectory != NULL) {
      m_ProjectConfig.ProjectDirectory = selectedDirectory;
    }
  }

  if (m_NoDirectorySelected) {
    ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4) ImColor(230, 57, 70));
    ImGui::Text("You must choose a project directoy!");
    ImGui::PopStyleColor();
  }

  if (m_DirectoryExists) {
    ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4) ImColor(230, 57, 70));
    ImGui::Text(
      "Folder with same name as project already exists at chosen directory!");
    ImGui::PopStyleColor();
  }

  if (ImGui::Button("Create Project")) {
    if (m_ProjectConfig.ProjectDirectory.empty()) {
      m_NoDirectorySelected = true;
    } else {
      if (std::filesystem::exists(m_ProjectConfig.ProjectDirectory)) {
        m_ProjectConfig.Name = m_ProjectNameInput;
        m_ProjectConfig.ProjectDirectory =
            m_ProjectConfig.ProjectDirectory / m_ProjectConfig.Name;
      }

      if (std::filesystem::is_directory(m_ProjectConfig.ProjectDirectory)) {
        m_DirectoryExists = true;
      }

      if (!m_DirectoryExists && !m_NoDirectorySelected) {
        Hamster::Project::New(m_ProjectConfig);

        std::string hamLibPath =
            Hamster::Application::GetExecutablePath() +
            "/../share/Resources/Hamster-Wheel/Resources/Packages/"
            HAMSTER_PY_MODULE_FILENAME;

        std::filesystem::copy(hamLibPath, m_ProjectConfig.ProjectDirectory);

        m_ProjectConfig = Hamster::ProjectConfig();
        ClosePanel();
      }
    }
  }

  ImGui::End();
}

//
// std::string ProjectCreator::OpenWindowsFileDialog(HWND owner) {
//     BROWSEINFO bi = {0};
//     bi.lpszTitle = "Select Directory";
//     bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
//     bi.hwndOwner = owner;
//
//     LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
//
//     if (pidl != 0) {
//         char path[MAX_PATH];
//         if (SHGetPathFromIDList(pidl, path)) {
//             // Free the PIDL allocated by SHBrowseForFolder
//             CoTaskMemFree(pidl);
//             return std::string(path);
//         }
//     }
//     return "";
// }

void CreateProject() {
}
