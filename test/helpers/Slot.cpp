#include "test/helpers/Slot.h"

Slot::Slot()
  : m_called(false)
{
}

void Slot::receive() {
  m_called = true;
}

bool Slot::called() {
  return m_called;
}

#include "Slot.moc"
