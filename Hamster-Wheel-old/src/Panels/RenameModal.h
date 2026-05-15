//
// Created by Jaden on 10/02/2025.
//

#ifndef RENAMEMODAL_H
#define RENAMEMODAL_H
#include "Gui/Modal.h"

#include <pybind11/attr.h>
#include <string>

class RenameModal {
public:
  RenameModal(std::string &name) : m_Name(name) {
    strcpy(m_NewName, m_Name.c_str());
  };

  void Render();

private:
  std::string &m_Name;
  char m_NewName[100];
};

#endif // RENAMEMODAL_H
