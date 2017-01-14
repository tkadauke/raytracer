#pragma once

/**
  * This class allows to execute a callback when the execution flow exits a
  * scope.
  */
class ScopeExit {
public:
  /**
    * Constructor. Accepts a lambda function which contains the code to execute
    * on scope exit.
    * 
    * @code
    * if (foo) {
    *   Object* obj = new Object;
    *   ScopeExit([=] { delete obj; });
    * }
    * @endcode
    */
  inline explicit ScopeExit(const std::function<void()>& func)
    : m_func(func)
  {
  }
  
  /**
    * Destructor. Executes the lambda function given in the constructor.
    */
  inline ~ScopeExit() {
    m_func();
  }

private:
  std::function<void()> m_func;
};
