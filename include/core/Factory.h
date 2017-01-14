#pragma once

#include <map>
#include <string>
#include <list>

/**
  * Generic factory class. Use this class to create instances of classes based
  * on an identifier, like a string.
  * 
  * @tparam Base the base class that all factory products share.
  * @tparam Identifier the type of object used to identify which object type to
  *   create.
  */
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
  /**
    * Constructor.
    */
  inline Factory() {}
  
  /**
    * Destructor. Cleans up the concrete factory objects.
    */
  inline ~Factory() {
    for (auto& i : m_creators) {
      delete i.second;
    }
  }
  
  /**
    * Registers the class @p Concrete with this factory. The identifier @p id
    * will be used to create objects of @p Concrete.
    * 
    * @returns true. This function has a return value, so it can be called on
    *   the module level.
    */
  template<class Concrete>
  inline bool registerClass(const Identifier& id) {
    m_creators[id] = new Creator<Concrete>;
    return true;
  }
  
  /**
    * Creates an object of the type associated with the identifier @p id.
    * 
    * @returns a pointer to the created object.
    */
  inline Base* create(const Identifier& id) const {
    if (const BaseCreator* creator = findCreator(id))
      return creator->create();
    else
      return nullptr;
  }

  /**
    * Creates an object of the type associated with the identifier @p id, and
    * puts it into a shared_ptr.
    * 
    * @returns the shared_ptr to the created object.
    */
  inline std::shared_ptr<Base> createShared(const Identifier& id) const {
    if (const BaseCreator* creator = findCreator(id))
      return std::shared_ptr<Base>(creator->create());
    else
      return nullptr;
  }
  
  /**
    * @returns a list of all registered identifiers.
    */
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
