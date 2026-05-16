//
// Created by jaden on 17/07/24.
//

#include "HamsterPCH.h"

#include "LayerStack.h"

namespace Hamster {

void LayerStack::PushLayer(Layer *layer) {
  layer->OnAttach();
  m_LayerStack.emplace_back(layer);
}

void LayerStack::PopLayer(Layer *layer) {
  auto it = std::find(m_LayerStack.begin(), m_LayerStack.end(), layer);

  if (it != m_LayerStack.end()) {
    layer->OnDetach();
    m_LayerStack.erase(it);
    delete layer;
  }
}

std::vector<Layer *>::iterator LayerStack::begin() {
  return m_LayerStack.begin();
}

std::vector<Layer *>::iterator LayerStack::end() { return m_LayerStack.end(); }

std::vector<Layer *>::iterator LayerStack::rend() {
  return m_LayerStack.begin();
}

std::vector<Layer *>::iterator LayerStack::rbegin() {
  return m_LayerStack.end();
}

std::vector<Layer *>::const_iterator LayerStack::begin() const {
  return m_LayerStack.begin();
}

std::vector<Layer *>::const_iterator LayerStack::end() const {
  return m_LayerStack.end();
}

std::vector<Layer *>::const_iterator LayerStack::rend() const {
  return m_LayerStack.begin();
}

std::vector<Layer *>::const_iterator LayerStack::rbegin() const {
  return m_LayerStack.end();
}
} // namespace Hamster
