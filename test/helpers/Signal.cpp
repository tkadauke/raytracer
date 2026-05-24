#include "test/helpers/Signal.h"

void Signal::call() {
  emit send();
}
