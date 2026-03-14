// vvar Tests - Comprehensive test suite for vvar (renamed from q3_util)
#define UTEST_USE_COLORS 1
#include "utest.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <type_traits>

#include "vvar_test_stubs.h"
#include "../vvar.h"
#include "../vvar_impl.h"

// Test helpers
static void resetVvarForTest() {
    vvarTestClearLog();
    veCVar::init();
    veCmd_InitDefaultFunctions();
    veGetCmd().getAliases().clear();
    veGetCmd().setWait(0);
}

static bool logContains(const char* text) {
    return vvarTestGetLog().find(text) != std::string::npos;
}

// ======================= Utility Functions Tests =======================

UTEST(veIsPrint, printable_chars) {
    resetVvarForTest();
    EXPECT_TRUE(veIsPrint('A'));
    EXPECT_TRUE(veIsPrint('z'));
    EXPECT_TRUE(veIsPrint('0'));
    EXPECT_TRUE(veIsPrint('!'));
    EXPECT_TRUE(veIsPrint('~'));
}

UTEST(veIsPrint, non_printable_chars) {
    resetVvarForTest();
    EXPECT_FALSE(veIsPrint(0));
    EXPECT_FALSE(veIsPrint('\n'));
    EXPECT_FALSE(veIsPrint('\t'));
    EXPECT_FALSE(veIsPrint(127));
}

UTEST(veIsLower, lowercase_letters) {
    resetVvarForTest();
    EXPECT_TRUE(veIsLower('a'));
    EXPECT_TRUE(veIsLower('z'));
    EXPECT_TRUE(veIsLower('m'));
}

UTEST(veIsLower, non_lowercase) {
    resetVvarForTest();
    EXPECT_FALSE(veIsLower('A'));
    EXPECT_FALSE(veIsLower('Z'));
    EXPECT_FALSE(veIsLower('0'));
    EXPECT_FALSE(veIsLower('!'));
}

UTEST(veIsUpper, uppercase_letters) {
    resetVvarForTest();
    EXPECT_TRUE(veIsUpper('A'));
    EXPECT_TRUE(veIsUpper('Z'));
    EXPECT_TRUE(veIsUpper('M'));
}

UTEST(veIsUpper, non_uppercase) {
    resetVvarForTest();
    EXPECT_FALSE(veIsUpper('a'));
    EXPECT_FALSE(veIsUpper('z'));
    EXPECT_FALSE(veIsUpper('0'));
    EXPECT_FALSE(veIsUpper('!'));
}

UTEST(veIsAlpha, alphabetic_chars) {
    resetVvarForTest();
    EXPECT_TRUE(veIsAlpha('a'));
    EXPECT_TRUE(veIsAlpha('z'));
    EXPECT_TRUE(veIsAlpha('A'));
    EXPECT_TRUE(veIsAlpha('Z'));
}

UTEST(veIsAlpha, non_alphabetic) {
    resetVvarForTest();
    EXPECT_FALSE(veIsAlpha('0'));
    EXPECT_FALSE(veIsAlpha('!'));
    EXPECT_FALSE(veIsAlpha(' '));
}

UTEST(veIsANumber, valid_integers) {
    resetVvarForTest();
    EXPECT_TRUE(veIsANumber("123"));
    EXPECT_TRUE(veIsANumber("-456"));
    EXPECT_TRUE(veIsANumber("0"));
    EXPECT_TRUE(veIsANumber("999999"));
}

UTEST(veIsANumber, valid_floats) {
    resetVvarForTest();
    EXPECT_TRUE(veIsANumber("1.5"));
    EXPECT_TRUE(veIsANumber("-3.14"));
    EXPECT_TRUE(veIsANumber("0.0"));
    EXPECT_TRUE(veIsANumber("123.456"));
}

UTEST(veIsANumber, invalid_strings) {
    resetVvarForTest();
    EXPECT_FALSE(veIsANumber(""));
    EXPECT_FALSE(veIsANumber("abc"));
    EXPECT_FALSE(veIsANumber("12a"));
    EXPECT_FALSE(veIsANumber("1.2.3"));
}

UTEST(veIsIntegral, whole_numbers) {
    resetVvarForTest();
    EXPECT_TRUE(veIsIntegral(0.0f));
    EXPECT_TRUE(veIsIntegral(1.0f));
    EXPECT_TRUE(veIsIntegral(-1.0f));
    EXPECT_TRUE(veIsIntegral(100.0f));
}

UTEST(veIsIntegral, decimal_numbers) {
    resetVvarForTest();
    EXPECT_FALSE(veIsIntegral(0.5f));
    EXPECT_FALSE(veIsIntegral(1.1f));
    EXPECT_FALSE(veIsIntegral(-2.7f));
    EXPECT_FALSE(veIsIntegral(3.14159f));
}

// ======================= veIVar Tests =======================

UTEST(veIVar, set_and_get) {
    resetVvarForTest();
    veIVar::set("player", "name", "test_player");
    const char* result = veIVar::get("player", "name");
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ("test_player", result);
}

UTEST(veIVar, get_nonexistent_key) {
    resetVvarForTest();
    const char* result = veIVar::get("nonexistent", "key");
    EXPECT_EQ(result, nullptr);
}

UTEST(veIVar, get_nonexistent_section) {
    resetVvarForTest();
    veIVar::set("player", "name", "test");
    const char* result = veIVar::get("other", "name");
    EXPECT_EQ(result, nullptr);
}

UTEST(veIVar, remove_existing) {
    resetVvarForTest();
    veIVar::set("player", "name", "test");
    veIVar::remove("player", "name");
    const char* result = veIVar::get("player", "name");
    EXPECT_EQ(result, nullptr);
}

UTEST(veIVar, remove_nonexistent) {
    resetVvarForTest();
    // Should not crash
    veIVar::remove("nonexistent", "key");
    SUCCEED();
}

UTEST(veIVar, overwrite_value) {
    resetVvarForTest();
    veIVar::set("player", "name", "first");
    veIVar::set("player", "name", "second");
    const char* result = veIVar::get("player", "name");
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ("second", result);
}

UTEST(veIVar, multiple_keys) {
    resetVvarForTest();
    veIVar::set("player", "name", "hero");
    veIVar::set("player", "health", "100");
    veIVar::set("weapon", "primary", "sword");
    EXPECT_STREQ("hero", veIVar::get("player", "name"));
    EXPECT_STREQ("100", veIVar::get("player", "health"));
    EXPECT_STREQ("sword", veIVar::get("weapon", "primary"));
}

UTEST(veIVar, to_string) {
    resetVvarForTest();
    veIVar::set("player", "name", "hero");
    veIVar::set("player", "rate", "25000");
    std::string info = veIVar::toString("player");
    EXPECT_TRUE(info.find("\\name\\hero") != std::string::npos);
    EXPECT_TRUE(info.find("\\rate\\25000") != std::string::npos);
}

UTEST(veIVar, from_string_round_trip) {
    resetVvarForTest();
    veIVar::fromString("player", "\\name\\hero\\rate\\25000");
    EXPECT_STREQ("hero", veIVar::get("player", "name"));
    EXPECT_STREQ("25000", veIVar::get("player", "rate"));
    std::string info = veIVar::toString("player");
    EXPECT_TRUE(info.find("\\name\\hero") != std::string::npos);
    EXPECT_TRUE(info.find("\\rate\\25000") != std::string::npos);
}

UTEST(veIVar, from_string_clears_existing_section) {
    resetVvarForTest();
    veIVar::set("player", "old", "value");
    veIVar::fromString("player", "\\name\\hero");
    EXPECT_EQ(veIVar::get("player", "old"), nullptr);
    EXPECT_STREQ("hero", veIVar::get("player", "name"));
}

// ======================= veCVar Tests =======================

UTEST(veCVar, get_create_new) {
    resetVvarForTest();
    veCVar* cv = veCVar::get("test_var", "default_value", 0);
    ASSERT_NE(cv, nullptr);
    EXPECT_STREQ("default_value", cv->getString().c_str());
}

UTEST(veCVar, get_existing) {
    resetVvarForTest();
    veCVar* cv1 = veCVar::get("test_var", "first_value", 0);
    veCVar* cv2 = veCVar::get("test_var", "second_value", 0);
    ASSERT_NE(cv1, nullptr);
    ASSERT_NE(cv2, nullptr);
    EXPECT_EQ(cv1, cv2);
}

UTEST(veCVarRef, resolves_lazily) {
    resetVvarForTest();
    veCVarRef ref("lazy_ref_test", "17", VE_CVAR_ARCHIVE);
    EXPECT_EQ(veCVar::find("lazy_ref_test"), nullptr);
    veCVar* resolved = ref.get();
    ASSERT_NE(resolved, nullptr);
    EXPECT_EQ(resolved, ref.get());
    EXPECT_STREQ("17", veCVar::variableString("lazy_ref_test"));
    EXPECT_TRUE(resolved->getFlags() & VE_CVAR_ARCHIVE);
}

UTEST(veCVar, get_null_parameters) {
    resetVvarForTest();
    // Should not crash with null parameters
    veCVar* cv = veCVar::get(nullptr, "value", 0);
    EXPECT_EQ(cv, nullptr);
    SUCCEED();
}

UTEST(veCVar, set_value) {
    resetVvarForTest();
    veCVar::get("test_var", "initial", 0);
    veCVar::set("test_var", "new_value");
    EXPECT_STREQ("new_value", veCVar::variableString("test_var"));
}

UTEST(veCVar, getBool) {
    resetVvarForTest();
    veCVar* cv = veCVar::get("bool_test", "1", 0);
    EXPECT_TRUE(cv->getBool());
    veCVar::set("bool_test", "0");
    EXPECT_FALSE(cv->getBool());
}

UTEST(veCVar, set2_force) {
    resetVvarForTest();
    veCVar::get("test_var", "initial", VE_CVAR_ROM);
    veCVar::set2("test_var", "forced_value", true);
    EXPECT_STREQ("forced_value", veCVar::variableString("test_var"));
}

UTEST(veCVar, set2_no_force_rom) {
    resetVvarForTest();
    veCVar::get("test_var", "initial", VE_CVAR_ROM);
    veCVar::set2("test_var", "new_value", false);
    EXPECT_STREQ("initial", veCVar::variableString("test_var"));
}

UTEST(veCVar, find_existing) {
    resetVvarForTest();
    veCVar::get("find_test", "value", 0);
    veCVar* found = veCVar::find("find_test");
    ASSERT_NE(found, nullptr);
    EXPECT_STREQ("value", found->getString().c_str());
}

UTEST(veCVar, find_nonexistent) {
    resetVvarForTest();
    veCVar* found = veCVar::find("nonexistent_var");
    EXPECT_EQ(found, nullptr);
}

UTEST(veCVar, variableValue) {
    resetVvarForTest();
    veCVar::get("test_num", "42.5", 0);
    EXPECT_NEAR(42.5f, veCVar::variableValue("test_num"), 0.001f);
}

UTEST(veCVar, variableIntegerValue) {
    resetVvarForTest();
    veCVar::get("test_int", "100", 0);
    EXPECT_EQ(100, veCVar::variableIntegerValue("test_int"));
}

UTEST(veCVar, variableString) {
    resetVvarForTest();
    veCVar::get("test_str", "hello world", 0);
    EXPECT_STREQ("hello world", veCVar::variableString("test_str"));
}

UTEST(veCVar, variableString_nonexistent) {
    resetVvarForTest();
    EXPECT_STREQ("", veCVar::variableString("nonexistent_var"));
}

UTEST(veCVar, setValue_float) {
    resetVvarForTest();
    veCVar::get("test_float", "0.0", 0);
    veCVar::setValue("test_float", 3.14159f);
    EXPECT_NEAR(3.14159f, veCVar::variableValue("test_float"), 0.001f);
}

UTEST(veCVar, setValue_integer) {
    resetVvarForTest();
    veCVar::get("test_int", "0", 0);
    veCVar::setValue("test_int", 42.0f);
    EXPECT_EQ(42, veCVar::variableIntegerValue("test_int"));
}

UTEST(veCVar, flags_basic) {
    resetVvarForTest();
    veCVar::get("test_flags", "0", VE_CVAR_ARCHIVE);
    int flags = veCVar::flags("test_flags");
    EXPECT_TRUE(flags & VE_CVAR_ARCHIVE);
}

UTEST(veCVar, flags_nonexistent) {
    resetVvarForTest();
    int flags = veCVar::flags("nonexistent_var");
    EXPECT_TRUE(flags & VE_CVAR_NONEXISTENT);
}

UTEST(veCVar, reset_to_default) {
    resetVvarForTest();
    veCVar::get("test_reset", "default", 0);
    veCVar::set("test_reset", "modified");
    veCVar::reset("test_reset");
    EXPECT_STREQ("default", veCVar::variableString("test_reset"));
}

UTEST(veCVar, forceReset) {
    resetVvarForTest();
    veCVar::get("test_force", "default", VE_CVAR_ROM);
    veCVar::set2("test_force", "modified", true);
    veCVar::reset("test_force");
    EXPECT_STREQ("modified", veCVar::variableString("test_force"));
    veCVar::forceReset("test_force");
    EXPECT_STREQ("default", veCVar::variableString("test_force"));
}

UTEST(veCVar, checkRange_float) {
    resetVvarForTest();
    veCVar* cv = veCVar::get("range_test", "5.0", 0);
    veCVar::checkRange(cv, 0.0f, 10.0f, false);
    veCVar::set("range_test", "-1.0");
    EXPECT_NEAR(0.0f, veCVar::variableValue("range_test"), 0.001f);
}

UTEST(veCVar, checkRange_integral) {
    resetVvarForTest();
    veCVar* cv = veCVar::get("integral_test", "5", 0);
    veCVar::checkRange(cv, 0.0f, 10.0f, true);
    veCVar::set("integral_test", "5.5");
    EXPECT_EQ(5, veCVar::variableIntegerValue("integral_test"));
}

UTEST(veCVar, checkRange_max_boundary) {
    resetVvarForTest();
    veCVar* cv = veCVar::get("max_test", "5.0", 0);
    veCVar::checkRange(cv, 0.0f, 10.0f, false);
    veCVar::set("max_test", "15.0");
    EXPECT_NEAR(10.0f, veCVar::variableValue("max_test"), 0.001f);
}

UTEST(veCVar, setDescription) {
    resetVvarForTest();
    veCVar* cv = veCVar::get("desc_test", "value", 0);
    veCVar::setDescription(cv, "Test description");
    EXPECT_STREQ("Test description", cv->getDescription().c_str());
}

UTEST(veCVar, restart_basic) {
    resetVvarForTest();
    veCVar::get("restart_test", "default", 0);
    veCVar::set("restart_test", "modified");
    veCVar::restart();
    EXPECT_STREQ("default", veCVar::variableString("restart_test"));
}

UTEST(veCVar, restart_no_restart_flag) {
    resetVvarForTest();
    veCVar::get("no_restart", "default", VE_CVAR_NORESTART);
    veCVar::set("no_restart", "modified");
    veCVar::restart();
    EXPECT_STREQ("modified", veCVar::variableString("no_restart"));
}

UTEST(veCVar, remove_existing) {
    resetVvarForTest();
    veCVar::get("remove_test", "value", 0);
    veCVar::remove("remove_test");
    veCVar* found = veCVar::find("remove_test");
    EXPECT_EQ(found, nullptr);
}

UTEST(veCVar, remove_nonexistent) {
    resetVvarForTest();
    // Should not crash
    veCVar::remove("nonexistent");
    SUCCEED();
}

UTEST(veCVar, command_print) {
    resetVvarForTest();
    veCVar::get("cmd_test", "value", 0);
    veGetCmd().executeString("cmd_test");
    // Command should print the variable value
    SUCCEED();
}

UTEST(veCVar, command_set) {
    resetVvarForTest();
    veCVar::get("cmd_set_test", "initial", 0);
    veGetCmd().executeString("cmd_set_test new_value");
    EXPECT_STREQ("new_value", veCVar::variableString("cmd_set_test"));
}

UTEST(veCVar, command_seta_flag) {
    resetVvarForTest();
    veCVar::get("seta_test", "value", 0);
    veGetCmd().executeString("seta seta_test new_value");
    veCVar* cv = veCVar::find("seta_test");
    ASSERT_NE(cv, nullptr);
    EXPECT_TRUE(cv->getFlags() & VE_CVAR_ARCHIVE);
}

UTEST(veCmd, alias_executes_sequence) {
    resetVvarForTest();
    veGetCmd().executeString("alias jump \"echo one; echo two\"");
    vvarTestClearLog();
    veGetCmd().executeString("jump");
    EXPECT_TRUE(logContains("one\n"));
    EXPECT_TRUE(logContains("two\n"));
}

UTEST(veCmd, exec_runs_script_file) {
    resetVvarForTest();
    const char* fileName = "vvar_exec_test.cfg";
    {
        std::ofstream file(fileName, std::ios::trunc);
        ASSERT_TRUE(file.good());
        file << "set exec_test success\n";
    }

    veGetCmd().executeString("exec vvar_exec_test.cfg");
    EXPECT_STREQ("success", veCVar::variableString("exec_test"));
    std::remove(fileName);
}

// ======================= veCmd Tests =======================

UTEST(veCmd, addCommand) {
    resetVvarForTest();
    bool called = false;
    veGetCmd().addCommand("testcmd", [&called]() { called = true; });
    veGetCmd().executeString("testcmd");
    EXPECT_TRUE(called);
}

UTEST(veCmd, addCommand_duplicate) {
    resetVvarForTest();
    int count = 0;
    veGetCmd().addCommand("dupcmd", [&count]() { count++; });
    veGetCmd().addCommand("dupcmd", [&count]() { count++; }); // Duplicate
    veGetCmd().executeString("dupcmd");
    EXPECT_EQ(count, 1);
}

UTEST(veCmd, removeCommand) {
    resetVvarForTest();
    bool called = false;
    veGetCmd().addCommand("removecmd", [&called]() { called = true; });
    veGetCmd().removeCommand("removecmd");
    veGetCmd().executeString("removecmd");
    EXPECT_FALSE(called);
}

UTEST(veCmd, tokenizeString_basic) {
    resetVvarForTest();
    veGetCmd().tokenizeString("test arg1 arg2");
    EXPECT_EQ(3, veGetCmd().argc());
    EXPECT_STREQ("test", veGetCmd().argv(0));
    EXPECT_STREQ("arg1", veGetCmd().argv(1));
    EXPECT_STREQ("arg2", veGetCmd().argv(2));
}

UTEST(veCmd, tokenizeString_quoted) {
    resetVvarForTest();
    veGetCmd().tokenizeString("test \"arg with spaces\"");
    EXPECT_EQ(2, veGetCmd().argc());
    EXPECT_STREQ("test", veGetCmd().argv(0));
    EXPECT_STREQ("arg with spaces", veGetCmd().argv(1));
}

UTEST(veCmd, tokenizeString_empty_args) {
    resetVvarForTest();
    veGetCmd().tokenizeString("test");
    EXPECT_EQ(1, veGetCmd().argc());
    EXPECT_STREQ("test", veGetCmd().argv(0));
}

UTEST(veCmd, tokenizeString_multiple_spaces) {
    resetVvarForTest();
    veGetCmd().tokenizeString("test   arg1    arg2");
    EXPECT_EQ(3, veGetCmd().argc());
    EXPECT_STREQ("test", veGetCmd().argv(0));
    EXPECT_STREQ("arg1", veGetCmd().argv(1));
    EXPECT_STREQ("arg2", veGetCmd().argv(2));
}

UTEST(veCmd, args) {
    resetVvarForTest();
    veGetCmd().tokenizeString("cmd arg1 arg2 arg3");
    std::string allArgs = veGetCmd().args();
    EXPECT_STREQ("arg1 arg2 arg3", allArgs.c_str());
}

UTEST(veCmd, argsFrom) {
    resetVvarForTest();
    veGetCmd().tokenizeString("cmd arg1 arg2 arg3");
    std::string fromArg2 = veGetCmd().argsFrom(2);
    EXPECT_STREQ("arg2 arg3", fromArg2.c_str());
}

UTEST(veCmd, executeString_command) {
    resetVvarForTest();
    bool called = false;
    veGetCmd().addCommand("exec_test", [&called]() { called = true; });
    veGetCmd().executeString("exec_test");
    EXPECT_TRUE(called);
}

UTEST(veCmd, executeString_multiple_commands) {
    resetVvarForTest();
    int count = 0;
    veGetCmd().addCommand("count", [&count]() { count++; });
    veGetCmd().executeString("count; count; count");
    EXPECT_EQ(3, count);
}

UTEST(veCmd, executeString_empty) {
    resetVvarForTest();
    veGetCmd().executeString("");
    SUCCEED();
}

UTEST(veCmd, executeString_whitespace_only) {
    resetVvarForTest();
    veGetCmd().executeString("   ");
    SUCCEED();
}

UTEST(veCmd, stringContains) {
    resetVvarForTest();
    const char* result = veCmd::stringContains("hello world", "world");
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ("world", result);
}

UTEST(veCmd, stringContains_case_sensitive) {
    resetVvarForTest();
    const char* result = veCmd::stringContains("hello World", "world", true);
    EXPECT_EQ(result, nullptr);
}

UTEST(veCmd, stringContains_case_insensitive) {
    resetVvarForTest();
    const char* result = veCmd::stringContains("hello World", "world", false);
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ("World", result);
}

UTEST(veCmd, filter_wildcard) {
    resetVvarForTest();
    EXPECT_TRUE(veCmd::filter("*", "anything"));
    EXPECT_TRUE(veCmd::filter("test*", "test123"));
    EXPECT_TRUE(veCmd::filter("*test", "mytest"));
    EXPECT_FALSE(veCmd::filter("test*", "other"));
}

UTEST(veCmd, filter_question_mark) {
    resetVvarForTest();
    EXPECT_TRUE(veCmd::filter("test?", "test1"));
    EXPECT_TRUE(veCmd::filter("test?", "test5"));
    EXPECT_FALSE(veCmd::filter("test?", "test"));
    EXPECT_FALSE(veCmd::filter("test?", "test12"));
}

UTEST(veCmd, filter_case_insensitive) {
    resetVvarForTest();
    EXPECT_TRUE(veCmd::filter("Test", "test", false));
    EXPECT_TRUE(veCmd::filter("test", "TEST", false));
}

// ======================= Built-in Commands Tests =======================

UTEST(veCmd, wait_command) {
    resetVvarForTest();
    veGetCmd().executeString("wait");
    EXPECT_GE(veGetCmd().getWait(), 1);
}

UTEST(veCmd, wait_command_with_arg) {
    resetVvarForTest();
    veGetCmd().executeString("wait 5");
    EXPECT_EQ(veGetCmd().getWait(), 5);
}

UTEST(veCmd, echo_command) {
    resetVvarForTest();
    // Echo command just prints to stdout
    veGetCmd().executeString("echo test message");
    SUCCEED();
}

UTEST(veCmd, cmdlist_command) {
    resetVvarForTest();
    // cmdlist should list all commands
    veGetCmd().executeString("cmdlist");
    SUCCEED();
}

UTEST(veCVarCmd, toggle_basic) {
    resetVvarForTest();
    veCVar::get("toggle_test", "0", 0);
    veGetCmd().executeString("toggle toggle_test");
    EXPECT_STREQ("1", veCVar::variableString("toggle_test"));
}

UTEST(veCVarCmd, toggle_multiple_values) {
    resetVvarForTest();
    veCVar::get("toggle_multi", "0", 0);
    veGetCmd().executeString("toggle toggle_multi 0 1 2");
    EXPECT_STREQ("1", veCVar::variableString("toggle_multi"));
}

UTEST(veCVarCmd, set_basic) {
    resetVvarForTest();
    veCVar::get("set_test", "initial", 0);
    veGetCmd().executeString("set set_test new_value");
    EXPECT_STREQ("new_value", veCVar::variableString("set_test"));
}

UTEST(veCVarCmd, seta_command) {
    resetVvarForTest();
    veCVar::get("seta_test", "value", 0);
    veGetCmd().executeString("seta seta_test new_value");
    veCVar* cv = veCVar::find("seta_test");
    ASSERT_NE(cv, nullptr);
    EXPECT_TRUE(cv->getFlags() & VE_CVAR_ARCHIVE);
}

UTEST(veCVarCmd, sets_command) {
    resetVvarForTest();
    veCVar::get("sets_test", "value", 0);
    veGetCmd().executeString("sets sets_test new_value");
    veCVar* cv = veCVar::find("sets_test");
    ASSERT_NE(cv, nullptr);
    EXPECT_TRUE(cv->getFlags() & VE_CVAR_SERVERINFO);
}

UTEST(veCVarCmd, setu_command) {
    resetVvarForTest();
    veCVar::get("setu_test", "value", 0);
    veGetCmd().executeString("setu setu_test new_value");
    veCVar* cv = veCVar::find("setu_test");
    ASSERT_NE(cv, nullptr);
    EXPECT_TRUE(cv->getFlags() & VE_CVAR_USERINFO);
}

UTEST(veCVarCmd, reset_command) {
    resetVvarForTest();
    veCVar::get("reset_cmd_test", "default", 0);
    veCVar::set("reset_cmd_test", "modified");
    veGetCmd().executeString("reset reset_cmd_test");
    EXPECT_STREQ("default", veCVar::variableString("reset_cmd_test"));
}

UTEST(veCVarCmd, cvarlist_command) {
    resetVvarForTest();
    veCVar::get("cvarlist_test", "value", 0);
    veGetCmd().executeString("cvarlist");
    SUCCEED();
}

UTEST(veCVarCmd, cvar_restart_command) {
    resetVvarForTest();
    veCVar::get("restart_cmd_test", "default", 0);
    veCVar::set("restart_cmd_test", "modified");
    veGetCmd().executeString("cvar_restart");
    EXPECT_STREQ("default", veCVar::variableString("restart_cmd_test"));
}

// ======================= Edge Cases and Error Handling =======================

UTEST(veCVar, invalid_name_characters) {
    resetVvarForTest();
    // The code checks for \ and " in cvar names
    veCVar* cv = veCVar::get("test\\var", "value", 0);
    ASSERT_NE(cv, nullptr);
    EXPECT_STREQ("BADNAME", cv->getString().c_str());
}

UTEST(veCmd, empty_string_tokenize) {
    resetVvarForTest();
    veGetCmd().tokenizeString("");
    EXPECT_EQ(0, veGetCmd().argc());
}

UTEST(veCmd, tokenize_with_comments) {
    resetVvarForTest();
    veGetCmd().tokenizeString("test arg1 // this is a comment");
    EXPECT_STREQ("test", veGetCmd().argv(0));
}

UTEST(veCVar, setLatched) {
    resetVvarForTest();
    veCVar::get("latched_test", "default", VE_CVAR_LATCH);
    veCVar::setLatched("latched_test", "pending");
    // Value should not change immediately
    EXPECT_STREQ("default", veCVar::variableString("latched_test"));
}

UTEST(veCVar, latched_value_after_restart) {
    resetVvarForTest();
    veCVar::get("latched_restart", "default", VE_CVAR_LATCH);
    veCVar::setLatched("latched_restart", "pending");
    veCVar::restart();
    EXPECT_STREQ("pending", veCVar::variableString("latched_restart"));
}

UTEST(veCVar, getModifiedFlags) {
    resetVvarForTest();
    int initialFlags = veCVar::getModifiedFlags();
    veCVar::get("modified_flags_test", "value", VE_CVAR_ARCHIVE);
    int newFlags = veCVar::getModifiedFlags();
    EXPECT_TRUE(newFlags & VE_CVAR_ARCHIVE);
}

UTEST(veCVar, updateFromIntegerFloatValues) {
    resetVvarForTest();
    veCVar* cv = veCVar::get("update_test", "5", VE_CVAR_ALLOW_SET_INTEGER);
    cv->getInteger() = 10;
    veCVar::updateFromIntegerFloatValues();
    EXPECT_EQ(10, veCVar::variableIntegerValue("update_test"));
    EXPECT_STREQ("10", veCVar::variableString("update_test"));

    cv->getValue() = 2.5f;
    veCVar::updateFromIntegerFloatValues();
    EXPECT_NEAR(2.5f, veCVar::variableValue("update_test"), 0.001f);
    EXPECT_STREQ("2.500000", veCVar::variableString("update_test"));
}

UTEST(veCmd, tokenizeStringIgnoreQuotes) {
    resetVvarForTest();
    veGetCmd().tokenizeStringIgnoreQuotes("test \"arg1 arg2\"");
    EXPECT_EQ(3, veGetCmd().argc());
    EXPECT_STREQ("test", veGetCmd().argv(0));
    EXPECT_STREQ("\"arg1", veGetCmd().argv(1));
    EXPECT_STREQ("arg2\"", veGetCmd().argv(2));
}

UTEST(veCmd, commandCompletion) {
    resetVvarForTest();
    int callbackCount = 0;
    veGetCmd().addCommand("completion_test", [](){});
    veGetCmd().commandCompletion([&callbackCount](const char* s, const char* expr) {
        callbackCount++;
    });
    EXPECT_TRUE(callbackCount > 0);
}

UTEST(veCVar, writeVariables) {
    resetVvarForTest();
    veCVar::get("write_test", "value", VE_CVAR_ARCHIVE);
    veFileData data;
    veCVar::writeVariables(data);
    // Should have written something
    EXPECT_TRUE(data.size() > 0);
}

UTEST(veCVar, print_modified_cvar) {
    resetVvarForTest();
    veCVar::get("print_test", "default", 0);
    veCVar::set("print_test", "modified");
    veCVar* cv = veCVar::find("print_test");
    ASSERT_NE(cv, nullptr);
    veCVar::print(cv);
    SUCCEED();
}

UTEST(veCVar, list_modified) {
    resetVvarForTest();
    veCVar::get("list_modified_clean", "default", 0);
    veCVar::get("list_modified_test", "default", 0);
    veCVar::set("list_modified_test", "modified");
    vvarTestClearLog();
    veCVar::listModified();
    EXPECT_TRUE(logContains("list_modified_test"));
    EXPECT_FALSE(logContains("list_modified_clean"));
}

UTEST(veCmd, executeString_cvar_command) {
    resetVvarForTest();
    veCVar::get("cvar_cmd_test", "initial", 0);
    veGetCmd().executeString("cvar_cmd_test");
    // Should print the variable
    SUCCEED();
}

UTEST(veCVar, cheat_protected) {
    resetVvarForTest();
    // sv_cheats is set to 0 by default (ROM)
    veCVar::get("cheat_test", "value", VE_CVAR_CHEAT);
    veGetCmd().executeString("cheat_test new_value");
    // Should not change because cheats are disabled
    EXPECT_STREQ("value", veCVar::variableString("cheat_test"));
}

UTEST(veCVar, read_only) {
    resetVvarForTest();
    veCVar::get("readonly_test", "initial", VE_CVAR_ROM);
    veGetCmd().executeString("readonly_test modified");
    EXPECT_STREQ("initial", veCVar::variableString("readonly_test"));
}

// ======================= veq3_va Tests =======================

UTEST(veq3_va, basic_format) {
    resetVvarForTest();
    char* result = veq3_va("test %d %s", 42, "hello");
    EXPECT_STREQ("test 42 hello", result);
}

UTEST(veq3_va, multiple_calls) {
    resetVvarForTest();
    char* r1 = veq3_va("first %d", 1);
    char* r2 = veq3_va("second %d", 2);
    EXPECT_STREQ("first 1", r1);
    EXPECT_STREQ("second 2", r2);
}

UTEST(veq3_va, nested_calls) {
    resetVvarForTest();
    char* result = veq3_va("outer %s", veq3_va("inner %d", 7));
    EXPECT_STREQ("outer inner 7", result);
}

// ======================= veCVar Constructor/Destructor =======================

UTEST(veCVar, constructor_is_internal) {
    resetVvarForTest();
    EXPECT_FALSE((std::is_default_constructible_v<veCVar>));
}

// ======================= veCmd Constructor/Destructor =======================

UTEST(veCmd, constructor) {
    resetVvarForTest();
    veCmd* cmd = new veCmd();
    ASSERT_NE(cmd, nullptr);
    delete cmd;
}

// ======================= Memory and Performance =======================

UTEST(veCVar, many_variables) {
    resetVvarForTest();
    for (int i = 0; i < 100; i++) {
        char name[32];
        std::snprintf(name, sizeof(name), "var_%d", i);
        veCVar::get(name, "value", 0);
    }
    // Should handle 100 cvars without issues
    veCVar* cv = veCVar::find("var_99");
    ASSERT_NE(cv, nullptr);
    EXPECT_STREQ("value", cv->getString().c_str());
}

UTEST(veCmd, many_commands) {
    resetVvarForTest();
    for (int i = 0; i < 100; i++) {
        char name[32];
        std::snprintf(name, sizeof(name), "cmd_%d", i);
        veGetCmd().addCommand(name, [](){});
    }
    // Should handle 100 commands without issues
    SUCCEED();
}

// ======================= Memory deallocation tests =======================

UTEST(veCVar, get_set_string_memory) {
    resetVvarForTest();
    veCVar::get("mem_test", "short", 0);
    veCVar::set("mem_test", "a much longer string with many characters to test memory handling");
    EXPECT_STREQ("a much longer string with many characters to test memory handling", veCVar::variableString("mem_test"));
}

UTEST(veCVar, empty_string_value) {
    resetVvarForTest();
    veCVar::get("empty_test", "", 0);
    EXPECT_STREQ("", veCVar::variableString("empty_test"));
}

// ======================= Command buffer tests =======================

// ======================= veCVar flag combinations =======================

UTEST(veCVar, multiple_flags) {
    resetVvarForTest();
    int flags = VE_CVAR_ARCHIVE | VE_CVAR_USERINFO | VE_CVAR_SERVERINFO;
    veCVar::get("multi_flags", "value", flags);
    veCVar* cv = veCVar::find("multi_flags");
    ASSERT_NE(cv, nullptr);
    EXPECT_TRUE(cv->getFlags() & VE_CVAR_ARCHIVE);
    EXPECT_TRUE(cv->getFlags() & VE_CVAR_USERINFO);
    EXPECT_TRUE(cv->getFlags() & VE_CVAR_SERVERINFO);
}

// ======================= veCVar modification tracking =======================

UTEST(veCVar, modification_count) {
    resetVvarForTest();
    veCVar* cv = veCVar::get("mod_count_test", "initial", 0);
    int initialModCount = cv->getModificationCount();
    veCVar::set("mod_count_test", "modified1");
    veCVar::set("mod_count_test", "modified2");
    EXPECT_EQ(initialModCount + 2, cv->getModificationCount());
}

UTEST(veCVar, modified_flag) {
    resetVvarForTest();
    veCVar* cv = veCVar::get("modified_flag_test", "initial", 0);
    EXPECT_FALSE(cv->getModified());
    veCVar::set("modified_flag_test", "modified");
    EXPECT_TRUE(cv->getModified());
}

// ======================= veIVar edge cases =======================

UTEST(veIVar, empty_section) {
    resetVvarForTest();
    veIVar::set("", "key", "value");
    const char* result = veIVar::get("", "key");
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ("value", result);
}

UTEST(veIVar, empty_key) {
    resetVvarForTest();
    veIVar::set("section", "", "value");
    const char* result = veIVar::get("section", "");
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ("value", result);
}

// ======================= veCmd tokenization edge cases =======================

UTEST(veCmd, tokenize_single_quotes) {
    resetVvarForTest();
    veGetCmd().tokenizeString("test 'arg with spaces'");
    EXPECT_EQ(2, veGetCmd().argc());
}

UTEST(veCmd, tokenize_empty_quotes) {
    resetVvarForTest();
    veGetCmd().tokenizeString("test \"\"");
    EXPECT_EQ(2, veGetCmd().argc());
    EXPECT_STREQ("", veGetCmd().argv(1));
}

// ======================= sv_cheats global variable =======================

UTEST(sv_cheats, is_initialized) {
    resetVvarForTest();
    ASSERT_NE(sv_cheats, nullptr);
    EXPECT_STREQ("sv_cheats", sv_cheats->getName().c_str());
}

// ======================= global g_cmd initialization =======================

UTEST(g_cmd, is_initialized) {
    resetVvarForTest();
    ASSERT_NE(g_cmd.get(), nullptr);
}

UTEST_MAIN()
