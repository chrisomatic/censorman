
#define test_print(name, condition) logi("   %-25s: %s", (name), (condition) ? "Pass" : "Fail");

static s32 vec3_compare_y(void *a, void *b)
{
    Vec3 *va = (Vec3 *)a;
    Vec3 *vb = (Vec3 *)b;

    return (va->y > vb->y) - (va->y < vb->y);
}

static s32 vec3_compare_z(void *a, void *b)
{
    Vec3 *va = (Vec3 *)a;
    Vec3 *vb = (Vec3 *)b;

    return (va->z > vb->z) - (va->z < vb->z);
}

b32 base_tests_run()
{
    {
        u32 capacity = KB(16);

        // arenas
        logi("=== ARENA TESTS ===");
        Arena *my_arena = arena_create(capacity);

        logi(":: Basic allocation");
        int* i = PUSH_ONE(my_arena, int);
        float* f = PUSH_ONE(my_arena, float);

        *i = 105;
        *f = 15.6f;

        String new_string = string_format(my_arena,"wow this is so cool! %d, %f", *i, *f);

        test_print("String equal", string_equal(new_string, S("wow this is so cool! 105, 15.600000")));
        test_print("Offset 1", (my_arena->offset == 35 + sizeof(int) + sizeof(float)));

        logi(":: Resetting");
        arena_reset(my_arena);
        test_print("Offset 2", my_arena->offset == 0);

        logi(":: Growth");

        u8* big_chunk = PUSH_ARRAY(my_arena, u8, capacity - 1);
        test_print("Big chunk check", (my_arena->offset == capacity - 1));
        test_print("Null check", (my_arena->next == NULL));

        u8* more_data = PUSH_ARRAY(my_arena, u8, 100);
        test_print("Capacity check", (my_arena->offset != my_arena->capacity));
        test_print("Next arena check", (my_arena->next != NULL));
        test_print("Next arena size check", (my_arena->next && my_arena->next->capacity == 2*my_arena->capacity));
        test_print("Next arena offset check", (my_arena->next && my_arena->next->offset == 100));

        u8* one = PUSH_ONE(my_arena, u8);
        logi(":: First available space");
        test_print("first arena check", (my_arena->offset == my_arena->capacity));

        logi(":: Resetting 2");
        arena_reset(my_arena);
        test_print("Offset check1", my_arena->offset == 0);
        test_print("Offset check2", (my_arena->next && my_arena->next->offset == 0));

        arena_destroy(my_arena);

        test_print("Arena Destroy", (my_arena == NULL));

        logi("======================");
    }

    // Strings

    {
        Temp scratch = scratch_begin();

        String s = S("Hello, World!");
        string_print(s);

        String t = string_format(scratch.arena,"The current line is %d", __LINE__);
        string_print(t);

        String u = string_concat(scratch.arena, 2, s,t);
        string_print(u);

        String v = string_concat(scratch.arena, 3, S("Test "),S("And "), S("Stuff"));
        string_print(v);

        StringList sl = string_list_create(scratch.arena);

        string_list_add(&sl, S("my "));
        string_list_add(&sl, S("name "));
        string_list_add(&sl, S("is "));
        string_list_add(&sl, S("chris,"));

        for(int i = 0; i < 10; ++i)
        {
            string_list_addf(&sl, "hello%d,",i);
        }

        string_print(string_list_collapse(&sl));

        string_list_remove(&sl, 13);
        string_print(string_list_collapse(&sl));

        String str = S("120, 5.0, \"Chris\", true");
        StringArray sa = string_split(scratch.arena, str, S(","));

        for(int i = 0; i < sa.count; ++i)
        {
            string_print(sa.items[i]);
        }

        String trim_test1 = S("    This is a test string with spaces at beginning!");
        String trim_test2 = S("This is a test string with spaces at   end!      ");
        String trim_test3 = S("       This has spaces on both sides!    ");
        String trim_test4 = S(" \t     This has lots of different whitespace! \r \t\t  ");

        trim_test1 = string_trim(trim_test1);
        trim_test2 = string_trim(trim_test2);
        trim_test3 = string_trim(trim_test3);
        trim_test4 = string_trim(trim_test4);

        string_print(trim_test1);
        string_print(trim_test2);
        string_print(trim_test3);
        string_print(trim_test4);

        logi("Trim test 1 len: %d", trim_test1.len);
        logi("Trim test 2 len: %d", trim_test2.len);
        logi("Trim test 3 len: %d", trim_test3.len);
        logi("Trim test 4 len: %d", trim_test4.len);

        String long_string = S("This is a long string, to test the string functions...");

        test_print("String starts with 1", string_starts_with(long_string, S("This is a long string")));
        test_print("String starts with 2", !string_starts_with(long_string, S("This is a long stringg")));
        test_print("String starts with 3", !string_starts_with(long_string, S("This is a short string")));

        test_print("String ends with 1", string_ends_with(long_string, S("string functions...")));
        test_print("String ends with 2", !string_ends_with(long_string, S("string functions....")));
        test_print("String ends with 3", !string_ends_with(long_string, S("text functions")));

        String base_contains = S("Oh, hey there!   hey bud, how are you?");
        test_print("Base Contains 1", string_contains(base_contains, S(" hey bud")));
        test_print("Base Contains 2", !string_contains(base_contains, S(" hey buddy")));

        String replace_str = S("Hi, my name is Chris. And I like Chris as my name.");
        String new_str = string_replace(scratch.arena, replace_str, S("Chris"), S("Joe"));

        string_print(replace_str);
        string_print(new_str);
        logi("Old string len: %u, new string len: %u", replace_str.len, new_str.len);

        scratch_end(scratch);
    }

    // Generic Lists
    {
        Temp scratch = scratch_begin();

        List list = list_create(scratch.arena, sizeof(Vec3));

        list_add(&list, &VEC3(1,2,3));
        list_add(&list, &VEC3(-2, 14.2, 20.4));
        list_add(&list, &VEC3(19, 20.1, 300));
        list_add(&list, &VEC3(-30.2, 25.0, 200));
        list_add(&list, &VEC3(-30.2, 7.7, -10.2));
        list_add(&list, &VEC3(30.0, 19.7, -0.2));
        list_add(&list, &VEC3(30.0, -1.7, 0.0));

        list_print(&list);
        list_remove(&list, 1);
        list_print(&list);

        Vec3 *a = list_get(&list, 1);
        vec3_print(*a, "A");

        ListArray arr = list_to_array(&list);
        Vec3 *vec = arr.items;

        list_array_sort(&arr, vec3_compare_y);
        for(int i = 0; i < arr.count; ++i)
        {
            vec3_print(vec[i], "Vector");
        }

        logi("");

        list_array_sort(&arr, vec3_compare_z);
        for(int i = 0; i < arr.count; ++i)
        {
            vec3_print(vec[i], "Vector");
        }

        scratch_end(scratch);
    }

    // Command Line

    {
        Temp scratch = scratch_begin();

        char *args[] = {"towers","something.txt","-s","assets/custom.settings","--debug","--mode","release","-f","bloom,grayscale"};
        int argc = ARRAY_COUNT(args);

        CmdLine cmdline = cmdline_parse(scratch.arena, argc, args);
        cmdline_print(&cmdline);

        b32    debug        = cmdline_has_flag(&cmdline, S("debug"));
        String input_file   = cmdline_get_unflagged(&cmdline,0);
        String sval         = cmdline_get_value(&cmdline, S("s"));
        String mode         = cmdline_get_value(&cmdline, S("mode"));
        String filter_str   = cmdline_get_value(&cmdline, S("f"));
        StringArray filters = string_split(scratch.arena, filter_str, S(","));

        logi("input file: " STR_FMT, STR_ARG(input_file));
        logi("sval:       " STR_FMT, STR_ARG(sval));
        logi("debug:      %s", STR_BOOL(debug));
        logi("mode:       " STR_FMT, STR_ARG(mode));

        logi("filters:    [");
        for(s32 i = 0; i < filters.count; ++i)
            logi("  [%d]: " STR_FMT, i, STR_ARG(filters.items[i]));
        logi("]");

        scratch_end(scratch);
    }

    return true;
}
