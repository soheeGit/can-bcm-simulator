/* 아주 작은 단위 테스트 프레임워크 (JUnit의 assertEquals 같은 역할)
 *
 * 사용법:
 *   static void test_something(void) {
 *       ASSERT_EQ(4, add(2, 2));
 *       ASSERT_TRUE(is_ok());
 *   }
 *
 *   int main(void) {
 *       RUN_TEST(test_something);
 *       return TEST_SUMMARY();     // 실패가 있으면 1, 없으면 0
 *   }
 */
#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>

/* 테스트 파일 하나 = 실행 파일 하나라서, 파일마다 따로 센다 */
static int tf_passed = 0;
static int tf_failed = 0;

/* 기대값과 실제값이 다르면 파일:줄, 실패한 코드, 두 값을 10진수/16진수로 출력 */
#define ASSERT_EQ(expected, actual)                                            \
    do {                                                                       \
        long long e_ = (long long)(expected);                                  \
        long long a_ = (long long)(actual);                                    \
        if (e_ == a_) {                                                        \
            tf_passed++;                                                       \
        } else {                                                               \
            tf_failed++;                                                       \
            printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #actual);       \
            printf("         기대: %lld (0x%llX)\n", e_, (unsigned long long)e_); \
            printf("         실제: %lld (0x%llX)\n", a_, (unsigned long long)a_); \
        }                                                                      \
    } while (0)

/* 조건이 참이어야 함 */
#define ASSERT_TRUE(cond)                                                      \
    do {                                                                       \
        if (cond) {                                                            \
            tf_passed++;                                                       \
        } else {                                                               \
            tf_failed++;                                                       \
            printf("    FAIL %s:%d: %s 가 참이어야 함\n", __FILE__, __LINE__, #cond); \
        }                                                                      \
    } while (0)

/* 조건이 거짓이어야 함 */
#define ASSERT_FALSE(cond)                                                     \
    do {                                                                       \
        if (!(cond)) {                                                         \
            tf_passed++;                                                       \
        } else {                                                               \
            tf_failed++;                                                       \
            printf("    FAIL %s:%d: %s 가 거짓이어야 함\n", __FILE__, __LINE__, #cond); \
        }                                                                      \
    } while (0)

/* 테스트 함수 이름을 출력하고 실행 */
#define RUN_TEST(fn)                                                           \
    do {                                                                       \
        printf("  - %s\n", #fn);                                               \
        fn();                                                                  \
    } while (0)

/* 결과 요약을 출력하고, main의 반환값(종료 코드)으로 쓸 값을 돌려준다 */
#define TEST_SUMMARY()                                                         \
    (printf("  %d passed, %d failed\n", tf_passed, tf_failed),                 \
     tf_failed == 0 ? 0 : 1)

#endif
