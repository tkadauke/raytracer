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
    inline virtual ~BaseCreator() {}
    virtual Base* create() const = 0;
  };
  
  template<class Concrete>
  class Creator : public BaseCreator {
  public:
    inline virtual Base* create() const {
      return new Concrete;
    }
  };
  
public:
  inline Factory() {}
  inline ~Factory() {
    for (auto& i : m_creators) {
      delete i.second;
    }
  }
  
  template<class Concrete>
  inline bool registerClass(const Identifier& id) {
    m_creators[id] = new Creator<Concrete>;
    return true;
  }
  
  inline Base* create(const Identifier& id) const {
    if (const BaseCreator* creator = findCreator(id))
      return creator->create();
    else
      return nullptr;
  }

  inline std::shared_ptr<Base> createShared(const Identifier& id) const {
    if (const BaseCreator* creator = findCreator(id))
      return std::shared_ptr<Base>(creator->create());
    else
      return nullptr;
  }
  
  inline std::list<std::string> identifiers() const {
    std::list<std::string> result;
    for (const auto& i : m_creators) {
      result.push_back(i.first);
    }
    return result;
  }

private:
  const BaseCreator* findCreator(const Identifier& id) const {
    typename Creators::const_iterator it = m_creators.find(id);
    if (it != m_creators.end()) {
      return it->second;
    } else {
      return nullptr;
    }
  }
  
  Creators m_creators;
};
