#pragma once

class ScopeExit {
public:
  ScopeExit(const std::function<void()>& func)
    : m_func(func)
  {
  }
  
  ~ScopeExit() {
    m_func();
  }

private:
  std::function<void()> m_func;
};
