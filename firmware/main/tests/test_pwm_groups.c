#include "unity.h"
#include "task_pwm_driver.h"
#include "cJSON.h"
#include "config_store.h"

TEST_CASE("PWM group mapping test", "[pwm]"){
    const char* config_json = "{ \"pwm_groups\": [ { \"name\": \"test\", \"kind\": \"RGB\", \"map\": { \"R\": 0, \"G\": 1, \"B\": 2 } } ] }";
    cJSON* root = cJSON_Parse(config_json);
    config_set_root(root);
    pwm_groups_init_from_config();
    TEST_ASSERT_EQUAL_INT(1, pwm_groups_count());
    const pwm_group_t* group = pwm_groups_get(0);
    TEST_ASSERT_EQUAL_STRING("test", group->name);
    TEST_ASSERT_EQUAL_INT(PWMG_RGB, group->kind);
    TEST_ASSERT_EQUAL_INT(0, group->map_r);
    TEST_ASSERT_EQUAL_INT(1, group->map_g);
    TEST_ASSERT_EQUAL_INT(2, group->map_b);
    cJSON_Delete(root);
}
