// RUN: %check_clang_tidy %s modernize-replace-boost-bind %t

namespace boost {
template <typename T>
T bind(T t) { return t; }

namespace placeholders {
struct placeholder {};
static placeholder _1;
static placeholder _2;
} // namespace placeholders
} // namespace boost

namespace std {
template <typename T>
T bind(T t) { return t; }

namespace placeholders {
struct placeholder {};
static placeholder _1;
static placeholder _2;
} // namespace placeholders
} // namespace std

void foo(int x, int y) {}
int bar(int x) { return x; }

void test_boost_bind() {
  auto f1 = boost::bind(foo, 1, 2);
  // CHECK-MESSAGES: :[[@LINE-1]]:13: warning: use std::bind instead of boost::bind
  // CHECK-FIXES: auto f1 = std::bind(foo, 1, 2);

  auto f2 = boost::bind(bar, boost::placeholders::_1);
  // CHECK-MESSAGES: :[[@LINE-1]]:13: warning: use std::bind instead of boost::bind
  // CHECK-MESSAGES: :[[@LINE-2]]:23: warning: use std::placeholders::_1 instead of boost::placeholders::_1
  // CHECK-FIXES: auto f2 = std::bind(bar, std::placeholders::_1);
}

void test_placeholders_with_scope() {
  {
    auto f = boost::bind(foo, boost::placeholders::_1, boost::placeholders::_2);
    // CHECK-MESSAGES: :[[@LINE-1]]:14: warning: use std::bind instead of boost::bind
    // CHECK-MESSAGES: :[[@LINE-2]]:24: warning: add using directive for std::placeholders
    // CHECK-MESSAGES: :[[@LINE-3]]:24: warning: use _1 instead of boost::placeholders::_1
    // CHECK-MESSAGES: :[[@LINE-4]]:47: warning: use _2 instead of boost::placeholders::_2
    // CHECK-FIXES: {
    // CHECK-FIXES:     using namespace std::placeholders;
    // CHECK-FIXES:     auto f = std::bind(foo, _1, _2);
  }
}

void test_placeholders_no_scope() {
  auto f = boost::bind(foo, boost::placeholders::_1, boost::placeholders::_2);
  // CHECK-MESSAGES: :[[@LINE-1]]:12: warning: use std::bind instead of boost::bind
  // CHECK-MESSAGES: :[[@LINE-2]]:22: warning: use std::placeholders::_1 instead of boost::placeholders::_1
  // CHECK-MESSAGES: :[[@LINE-3]]:45: warning: use std::placeholders::_2 instead of boost::placeholders::_2
  // CHECK-FIXES: auto f = std::bind(foo, std::placeholders::_1, std::placeholders::_2);
}