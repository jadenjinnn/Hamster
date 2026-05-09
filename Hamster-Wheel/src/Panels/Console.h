#ifndef CONSOLE_H
#define CONSOLE_H

#include <memory>
#include <vector>

#include <Core/Log.h>
#include <Gui/Panel.h>

class Console : public Hamster::Panel {
public:
  explicit Console(Hamster::EventDispatcher *dispatcher,
                   std::shared_ptr<Hamster::Scene> scene);

  void Render() override;

private:
  std::shared_ptr<Hamster::Logger> m_ClientLogger;
};

#endif // !CONSOLE_H
