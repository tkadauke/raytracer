#pragma once

class ScopeExit {
public:
  inline explicit ScopeExit(const std::function<void()>& func)
    : m_func(func)
  {
  }
  
  inline ~ScopeExit() {
    m_func();
  }

private:
  std::function<void()> m_func;
};
