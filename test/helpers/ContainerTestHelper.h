#ifndef CONTAINER_TEST_HELPER_H
#define CONTAINER_TEST_HELPER_H

#include <vector>
#include <list>

namespace testing {
  namespace internal {
    template<class Container>
    bool containersEqual(const Container& expected, const Container& actual) {
      if (expected.size() != actual.size())
        return false;
      for (typename Container::const_iterator i = expected.begin(), j = actual.begin(); i != expected.end(); ++i, ++j) {
        if (!(*i == *j))
          return false;
      }
      return true;
    }
    
    template<class Ostream, class Container>
    void outputContainer(Ostream& os, const Container& container) {
      unsigned int count = 0;
      os << '[';
      for (const auto& element : container) {
        os << element;
        if (++count != container.size())
          os << ", ";
      }
      os << ']';
    }

    template<class Container>
    // Helper function for implementing ASSERT_CONTAINERS_EQ.
    AssertionResult ContainersEqPredFormat(
      const char* expr1,
      const char* expr2,
      const Container& val1,
      const Container& val2
    ) {
      if (containersEqual(val1, val2)) return AssertionSuccess();

      Message msg;
      msg << "The containers " << expr1 << " and " << expr2
          << " are not equal, where\n"
          << expr1 << " evaluates to ";
      outputContainer(msg, val1);
      msg << ",\n" << expr2 << " evaluates to ";
      outputContainer(msg, val2);
      msg << ".";
      return AssertionFailure(msg);
    }
  }

  template<class T>
  std::vector<T> makeStdVector(const T& v0) {
    return std::vector<T>({v0});
  }
  
  template<class T>
  std::vector<T> makeStdVector(const T& v0, const T& v1) {
    return std::vector<T>({v0, v1});
  }
  
  template<class T>
  std::vector<T> makeStdVector(const T& v0, const T& v1, const T& v2) {
    return std::vector<T>({v0, v1, v2});
  }
  
  template<class T>
  std::vector<T> makeStdVector(const T& v0, const T& v1, const T& v2, const T& v3) {
    return std::vector<T>({v0, v1, v2, v3});
  }
  
  template<class T>
  std::vector<T> makeStdVector(const T& v0, const T& v1, const T& v2, const T& v3, const T& v4) {
    return std::vector<T>({v0, v1, v2, v3, v4});
  }
  
  template<class T>
  std::list<T> makeStdList(const T& v0) {
    return std::list<T>({v0});
  }
  
  template<class T>
  std::list<T> makeStdList(const T& v0, const T& v1) {
    return std::list<T>({v0, v1});
  }
  
  template<class T>
  std::list<T> makeStdList(const T& v0, const T& v1, const T& v2) {
    return std::list<T>({v0, v1, v2});
  }
  
  template<class T>
  std::list<T> makeStdList(const T& v0, const T& v1, const T& v2, const T& v3) {
    return std::list<T>({v0, v1, v2, v3});
  }
}

#define ASSERT_CONTAINERS_EQ(val1, val2) \
  ASSERT_PRED_FORMAT2(::testing::internal::ContainersEqPredFormat, val1, val2)

#endif
