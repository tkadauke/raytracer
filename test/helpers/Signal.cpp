#include "test/helpers/Signal.h"

void Signal::call() {
  emit send();
}

#include "Signal.moc"
