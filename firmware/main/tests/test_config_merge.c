#include "unity.h"
#include "config_store.h"
#include "cJSON.h"
#include "storage_fs.h"

TEST_CASE("Config merge/persist test", "[config]"){
    storage_fs_init();
    const char* config_json = "{ \"test\": \"value\" }";
    cJSON* root = cJSON_Parse(config_json);
    config_set_root(root);
    config_save();
    cJSON_Delete(root);
    config_load();
    const cJSON* loaded_root = config_root();
    TEST_ASSERT_NOT_NULL(loaded_root);
    const cJSON* test_item = cJSON_GetObjectItem(loaded_root, "test");
    TEST_ASSERT_NOT_NULL(test_item);
    TEST_ASSERT_EQUAL_STRING("value", cJSON_GetStringValue(test_item));
}
