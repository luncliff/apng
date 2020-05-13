#include <gtest/gtest.h>

#include <clocale>

int main(int argc, char** argv) {
    const auto locale = ".65001";
    if (auto cat = setlocale(LC_ALL, locale))
        fprintf(stderr, "changed locale: %s >> %s \n", cat, locale);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(TestClass1, test_1) {
    ASSERT_EQ(0, 0); // ...
}

TEST(TestClass2, test_2) {
    ASSERT_EQ(1, 1); // ...
}
