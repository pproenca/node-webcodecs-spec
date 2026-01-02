/**
 * test_main.cpp - Google Test entry point for webcodecs C++ tests
 */

#include <gtest/gtest.h>

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
