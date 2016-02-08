#pragma once

#include <map>
#include <string>
#include <list>

template<class Base, class Identifier = std::string>
class Factory {
  class BaseCreator;
  typedef std::map<Identifier, BaseCreator*> Creators;
  
  class BaseCreator {
  public:
    virtual ~BaseCreator() {}
    virtual Base* create() = 0;
  };
  
  template<class Concrete>
  class Creator : public BaseCreator {
  public:
    virtual Base* create() {
      return new Concrete;
    }
  };
  
public:
  Factory() {}
  ~Factory() {
    for (auto& i : m_creators) {
      delete i.second;
    }
  }
  
  template<class Concrete>
  bool registerClass(const Identifier& id) {
    m_creators[id] = new Creator<Concrete>;
    return true;
  }
  
  Base* create(const Identifier& id) {
    if (BaseCreator* creator = m_creators[id])
      return creator->create();
    else
      return nullptr;
  }
  
  std::list<std::string> identifiers() const {
    std::list<std::string> result;
    for (const auto& i : m_creators) {
      result.push_back(i.first);
    }
    return result;
  }

private:
  Creators m_creators;
};
