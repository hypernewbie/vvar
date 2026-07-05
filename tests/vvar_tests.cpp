// vvar Tests - Comprehensive test suite for vvar.
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
    // Commands are not cleared by init(); addCommand ignores duplicates, so stale
    // lambdas (capturing dead stack state) would survive across tests. Clear them and
    // re-register the defaults to keep each test hermetic.
    veGetCmd().getFunctionsList().clear();
    // Drain any command text left buffered by a previous test. wait state can re-block
    // each frame, so clear it every iteration.
    for (int i = 0; i < 64; i++) {
        veGetCmd().setWait(0);
        veGetCmd().execute(VE_CMD_EXEC_NOW);
    }
    veGetCmd().setWait(0);
    veCVar_InitCmd();
    veGetCmd().addCommand("cmdlist", veCmd_ListFunc);
    veGetCmd().addCommand("echo", veCmd_EchoFunc);
    veGetCmd().addCommand("exec", veCmd_ExecFunc);
    veGetCmd().addCommand("wait", veCmd_WaitFunc);
    vvarTestClearLog();
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
    veGetCmd().commandCompletion([&callbackCount](const char*, const char*) {
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

// ======================= veExtractStartupCommands =======================

UTEST(veExtractStartupCommands, basic_extraction) {
    const char* argv[] = { "exe", "--null", "+set", "r_debug", "1", "+exec", "my.cfg" };
    std::vector< std::string > remaining, commands;
    veExtractStartupCommands( 7, argv, remaining, commands );

    ASSERT_EQ( 2u, commands.size() );
    EXPECT_STREQ( "set r_debug 1", commands[0].c_str() );
    EXPECT_STREQ( "exec my.cfg",   commands[1].c_str() );

    ASSERT_EQ( 2u, remaining.size() );
    EXPECT_STREQ( "exe",    remaining[0].c_str() );
    EXPECT_STREQ( "--null", remaining[1].c_str() );
}

UTEST(veExtractStartupCommands, no_commands) {
    const char* argv[] = { "exe", "--foo", "bar" };
    std::vector< std::string > remaining, commands;
    veExtractStartupCommands( 3, argv, remaining, commands );

    EXPECT_TRUE( commands.empty() );
    ASSERT_EQ( 3u, remaining.size() );
    EXPECT_STREQ( "exe",   remaining[0].c_str() );
    EXPECT_STREQ( "--foo", remaining[1].c_str() );
    EXPECT_STREQ( "bar",   remaining[2].c_str() );
}

UTEST(veExtractStartupCommands, adjacent_commands) {
    const char* argv[] = { "exe", "+foo", "+bar" };
    std::vector< std::string > remaining, commands;
    veExtractStartupCommands( 3, argv, remaining, commands );

    ASSERT_EQ( 2u, commands.size() );
    EXPECT_STREQ( "foo", commands[0].c_str() );
    EXPECT_STREQ( "bar", commands[1].c_str() );
    ASSERT_EQ( 1u, remaining.size() );
}

UTEST(veExtractStartupCommands, empty_argv) {
    std::vector< std::string > remaining, commands;
    veExtractStartupCommands( 0, nullptr, remaining, commands );

    EXPECT_TRUE( commands.empty() );
    EXPECT_TRUE( remaining.empty() );
}

UTEST(veExecuteStartupCommands, sets_cvar) {
    resetVvarForTest();
    const char* argv[] = { "exe", "+set", "startup_var", "42" };
    std::vector< std::string > remaining, commands;
    veExtractStartupCommands( 4, argv, remaining, commands );
    veExecuteStartupCommands( commands );

    EXPECT_EQ( 42, veCVar::variableIntegerValue( "startup_var" ) );
}

// =====================================================================================
// ============================ EXPANDED COVERAGE (Phase 1) =============================
// =====================================================================================
// These tests characterize the current observable behavior of the library so that the
// public API is locked down against regressions. They exercise only the public API
// and assert on observed results.

// ------------------------------- veIsANumber deep --------------------------------
UTEST(veIsANumber_x, leading_trailing_space) {
    resetVvarForTest();
    // strtod skips leading whitespace, so leading space parses; trailing junk fails
    EXPECT_TRUE(veIsANumber("  12"));
    EXPECT_FALSE(veIsANumber("12 "));
    EXPECT_FALSE(veIsANumber(" "));
}

UTEST(veIsANumber_x, scientific_and_signs) {
    resetVvarForTest();
    EXPECT_TRUE(veIsANumber("1e5"));
    EXPECT_TRUE(veIsANumber("-1.5e-3"));
    EXPECT_TRUE(veIsANumber("+3"));
    EXPECT_TRUE(veIsANumber(".5"));
    EXPECT_TRUE(veIsANumber("5."));
}

UTEST(veIsANumber_x, hex_and_inf_nan) {
    resetVvarForTest();
    // strtod parses these fully on typical libc
    EXPECT_TRUE(veIsANumber("0x1p4"));
    EXPECT_TRUE(veIsANumber("inf"));
    EXPECT_TRUE(veIsANumber("nan"));
    EXPECT_FALSE(veIsANumber("infinityx"));
}

UTEST(veIsIntegral_x, boundaries) {
    resetVvarForTest();
    EXPECT_TRUE(veIsIntegral(1000000.0f));
    EXPECT_TRUE(veIsIntegral(-42.0f));
    EXPECT_FALSE(veIsIntegral(0.0001f));
}

// ------------------------------- veIVar edge cases --------------------------------
UTEST(veIVar_x, tostring_empty_section_returns_empty) {
    resetVvarForTest();
    EXPECT_STREQ("", veIVar::toString("nope"));
}

UTEST(veIVar_x, tostring_null_section) {
    resetVvarForTest();
    // null section maps to the "" section; clear it first (veIVar table is global
    // and not reset by resetVvarForTest).
    veIVar::fromString(nullptr, "");
    EXPECT_STREQ("", veIVar::toString(nullptr));
}

UTEST(veIVar_x, remove_last_key_erases_section) {
    resetVvarForTest();
    veIVar::set("s", "k", "v");
    veIVar::remove("s", "k");
    EXPECT_STREQ("", veIVar::toString("s"));
    EXPECT_EQ(nullptr, veIVar::get("s", "k"));
}

UTEST(veIVar_x, fromstring_empty_string_clears) {
    resetVvarForTest();
    veIVar::set("s", "k", "v");
    veIVar::fromString("s", "");
    EXPECT_EQ(nullptr, veIVar::get("s", "k"));
}

UTEST(veIVar_x, fromstring_null_infostring_clears) {
    resetVvarForTest();
    veIVar::set("s", "k", "v");
    veIVar::fromString("s", nullptr);
    EXPECT_EQ(nullptr, veIVar::get("s", "k"));
}

UTEST(veIVar_x, fromstring_no_leading_backslash) {
    resetVvarForTest();
    veIVar::fromString("s", "name\\hero");
    EXPECT_STREQ("hero", veIVar::get("s", "name"));
}

UTEST(veIVar_x, fromstring_trailing_key_no_value) {
    resetVvarForTest();
    // "\a\1\b" -> b has no value, dropped; a=1 kept
    veIVar::fromString("s", "\\a\\1\\b");
    EXPECT_STREQ("1", veIVar::get("s", "a"));
    EXPECT_EQ(nullptr, veIVar::get("s", "b"));
}

UTEST(veIVar_x, fromstring_duplicate_keys_last_wins) {
    resetVvarForTest();
    veIVar::fromString("s", "\\k\\first\\k\\second");
    EXPECT_STREQ("second", veIVar::get("s", "k"));
}

UTEST(veIVar_x, tostring_skips_invalid_chars) {
    resetVvarForTest();
    // Set a value directly containing a quote/backslash/semicolon via map set
    veIVar::set("s", "good", "ok");
    veIVar::set("s", "bad", "a\"b");
    std::string out = veIVar::toString("s");
    EXPECT_TRUE(out.find("\\good\\ok") != std::string::npos);
    EXPECT_TRUE(out.find("bad") == std::string::npos);
}

// ------------------------------- veCVar get semantics --------------------------------
UTEST(veCVar_x, get_empty_value_does_not_override_existing) {
    resetVvarForTest();
    veCVar::get("v", "orig", 0);
    veCVar::get("v", "", 0);
    EXPECT_STREQ("orig", veCVar::variableString("v"));
}

UTEST(veCVar_x, get_ors_in_flags_on_reregister) {
    resetVvarForTest();
    veCVar::get("v", "1", VE_CVAR_ARCHIVE);
    veCVar* cv = veCVar::get("v", "1", VE_CVAR_USERINFO);
    EXPECT_TRUE(cv->getFlags() & VE_CVAR_ARCHIVE);
    EXPECT_TRUE(cv->getFlags() & VE_CVAR_USERINFO);
}

UTEST(veCVar_x, get_null_value_returns_null) {
    resetVvarForTest();
    EXPECT_EQ(nullptr, veCVar::get("v", nullptr, 0));
}

UTEST(veCVar_x, get_reports_conflicting_reset_value) {
    resetVvarForTest();
    veCVar::get("v", "a", 0);
    vvarTestClearLog();
    veCVar::get("v", "b", 0);
    EXPECT_TRUE(logContains("different") || logContains("initial values"));
}

UTEST(veCVar_x, get_applies_latched_string_on_reregister) {
    resetVvarForTest();
    veCVar::get("v", "def", VE_CVAR_LATCH);
    veCVar::setLatched("v", "new");
    EXPECT_STREQ("def", veCVar::variableString("v"));
    // Re-registering pulls in the latched value
    veCVar::get("v", "def", VE_CVAR_LATCH);
    EXPECT_STREQ("new", veCVar::variableString("v"));
}

// ------------------------------- set2 / latch / rom / init --------------------------------
UTEST(veCVar_x, set_same_value_no_modification) {
    resetVvarForTest();
    veCVar* cv = veCVar::get("v", "x", 0);
    int mc = cv->getModificationCount();
    veCVar::set("v", "x");
    EXPECT_EQ(mc, cv->getModificationCount());
}

UTEST(veCVar_x, init_flag_blocks_console_set) {
    resetVvarForTest();
    veCVar::get("v", "init", VE_CVAR_INIT);
    veCVar::set2("v", "changed", false);
    EXPECT_STREQ("init", veCVar::variableString("v"));
}

UTEST(veCVar_x, latch_double_set_updates_pending) {
    resetVvarForTest();
    veCVar::get("v", "def", VE_CVAR_LATCH);
    veCVar::setLatched("v", "one");
    veCVar::setLatched("v", "two");
    EXPECT_STREQ("def", veCVar::variableString("v"));
    veCVar::restart();
    EXPECT_STREQ("two", veCVar::variableString("v"));
}

UTEST(veCVar_x, latch_set_back_to_current_is_noop_quirk) {
    resetVvarForTest();
    veCVar::get("v", "def", VE_CVAR_LATCH);
    veCVar::setLatched("v", "one");
    // Setting latch back to the current string value short-circuits at the top-of-set2
    // equality check BEFORE the latch handling, so the pending "one" is NOT cleared.
    veCVar::setLatched("v", "def");
    veCVar::restart();
    EXPECT_STREQ("one", veCVar::variableString("v"));
}

UTEST(veCVar_x, force_set_clears_latch) {
    resetVvarForTest();
    veCVar::get("v", "def", VE_CVAR_LATCH);
    veCVar::setLatched("v", "pending");
    veCVar::set2("v", "forced", true);
    EXPECT_STREQ("forced", veCVar::variableString("v"));
    // restart() resets non-ROM/INIT/NORESTART vars: latch is cleared, so it falls back
    // to the reset string "def".
    veCVar::restart();
    EXPECT_STREQ("def", veCVar::variableString("v"));
}

UTEST(veCVar_x, rom_set_emits_message) {
    resetVvarForTest();
    veCVar::get("v", "ro", VE_CVAR_ROM);
    vvarTestClearLog();
    veCVar::set2("v", "x", false);
    EXPECT_TRUE(logContains("read only"));
}

// ------------------------------- cheats --------------------------------
UTEST(veCVar_x, cheat_set_allowed_when_cheats_on) {
    resetVvarForTest();
    veCVar::get("v", "safe", VE_CVAR_CHEAT);
    veCVar::set2("sv_cheats", "1", true);
    veCVar::set2("v", "hacked", false);
    EXPECT_STREQ("hacked", veCVar::variableString("v"));
}

UTEST(veCVar_x, setCheatState_resets_cheat_vars) {
    resetVvarForTest();
    veCVar::set2("sv_cheats", "1", true);
    veCVar::get("v", "safe", VE_CVAR_CHEAT);
    veCVar::set2("v", "hacked", false);
    EXPECT_STREQ("hacked", veCVar::variableString("v"));
    veCVar::setCheatState();
    EXPECT_STREQ("safe", veCVar::variableString("v"));
}

// ------------------------------- setSafe / protected --------------------------------
UTEST(veCVar_x, setSafe_blocks_protected) {
    resetVvarForTest();
    veCVar::get("v", "orig", VE_CVAR_PROTECTED);
    vvarTestClearLog();
    veCVar::setSafe("v", "evil");
    EXPECT_STREQ("orig", veCVar::variableString("v"));
    EXPECT_TRUE(logContains("Restricted source"));
}

UTEST(veCVar_x, setSafe_allows_normal) {
    resetVvarForTest();
    veCVar::get("v", "orig", 0);
    veCVar::setSafe("v", "ok");
    EXPECT_STREQ("ok", veCVar::variableString("v"));
}

// ------------------------------- validate / range --------------------------------
UTEST(veCVar_x, checkRange_min_boundary_inclusive) {
    resetVvarForTest();
    veCVar* cv = veCVar::get("v", "5", 0);
    veCVar::checkRange(cv, 0.0f, 10.0f, false);
    veCVar::set("v", "0");
    EXPECT_NEAR(0.0f, veCVar::variableValue("v"), 0.001f);
}

UTEST(veCVar_x, checkRange_non_numeric_falls_back_to_reset) {
    resetVvarForTest();
    veCVar* cv = veCVar::get("v", "5", 0);
    veCVar::checkRange(cv, 0.0f, 10.0f, false);
    veCVar::set("v", "abc");
    EXPECT_NEAR(5.0f, veCVar::variableValue("v"), 0.001f);
}

UTEST(veCVar_x, checkRange_integral_rounds_down) {
    resetVvarForTest();
    veCVar* cv = veCVar::get("v", "5", 0);
    veCVar::checkRange(cv, 0.0f, 100.0f, true);
    veCVar::set("v", "7.9");
    EXPECT_EQ(7, veCVar::variableIntegerValue("v"));
}

// ------------------------------- flags() modified bit --------------------------------
UTEST(veCVar_x, flags_modified_bit_set_after_change) {
    resetVvarForTest();
    veCVar::get("v", "a", 0);
    veCVar::set("v", "b");
    EXPECT_TRUE(veCVar::flags("v") & VE_CVAR_MODIFIED);
}

// ------------------------------- writeVariables --------------------------------
UTEST(veCVar_x, writeVariables_only_archive) {
    resetVvarForTest();
    veCVar::get("arch", "1", VE_CVAR_ARCHIVE);
    veCVar::get("noarch", "2", 0);
    veFileData data;
    veCVar::writeVariables(data);
    std::string s(data.begin(), data.end());
    EXPECT_TRUE(s.find("seta arch") != std::string::npos);
    EXPECT_TRUE(s.find("noarch") == std::string::npos);
}

UTEST(veCVar_x, writeVariables_uses_latched_value) {
    resetVvarForTest();
    veCVar::get("v", "def", VE_CVAR_ARCHIVE | VE_CVAR_LATCH);
    veCVar::setLatched("v", "pending");
    veFileData data;
    veCVar::writeVariables(data);
    std::string s(data.begin(), data.end());
    EXPECT_TRUE(s.find("pending") != std::string::npos);
}

// ------------------------------- command dispatch --------------------------------
UTEST(veCmd_x, unknown_command_reports) {
    resetVvarForTest();
    vvarTestClearLog();
    veGetCmd().executeString("no_such_command_xyz");
    EXPECT_TRUE(logContains("unknown command"));
}

UTEST(veCmd_x, cmd_returns_raw_input) {
    resetVvarForTest();
    veGetCmd().tokenizeString("foo bar baz");
    EXPECT_STREQ("foo bar baz", veGetCmd().cmd());
}

UTEST(veCmd_x, argv_out_of_range_returns_empty) {
    resetVvarForTest();
    veGetCmd().tokenizeString("one two");
    EXPECT_STREQ("", veGetCmd().argv(5));
    EXPECT_STREQ("", veGetCmd().argv(2));
}

UTEST(veCmd_x, argsFrom_negative_clamped_to_zero) {
    resetVvarForTest();
    veGetCmd().tokenizeString("a b c");
    std::string s = veGetCmd().argsFrom(-3);
    EXPECT_STREQ("a b c", s.c_str());
}

UTEST(veCmd_x, args_empty_when_single_token) {
    resetVvarForTest();
    veGetCmd().tokenizeString("solo");
    std::string s = veGetCmd().args();
    EXPECT_STREQ("", s.c_str());
}

// ------------------------------- tokenizer edge cases --------------------------------
UTEST(veCmd_x, tokenize_star_comment_block) {
    resetVvarForTest();
    veGetCmd().tokenizeString("a /* comment */ b");
    EXPECT_EQ(2, veGetCmd().argc());
    EXPECT_STREQ("a", veGetCmd().argv(0));
    EXPECT_STREQ("b", veGetCmd().argv(1));
}

UTEST(veCmd_x, tokenize_unterminated_quote) {
    resetVvarForTest();
    veGetCmd().tokenizeString("a \"unterminated");
    EXPECT_EQ(2, veGetCmd().argc());
    EXPECT_STREQ("unterminated", veGetCmd().argv(1));
}

UTEST(veCmd_x, tokenize_tabs_as_whitespace) {
    resetVvarForTest();
    veGetCmd().tokenizeString("a\tb\tc");
    EXPECT_EQ(3, veGetCmd().argc());
}

UTEST(veCmd_x, tokenize_null_text_no_args) {
    resetVvarForTest();
    veGetCmd().tokenizeString(nullptr);
    EXPECT_EQ(0, veGetCmd().argc());
}

UTEST(veCmd_x, tokenize_quote_breaks_regular_token) {
    resetVvarForTest();
    veGetCmd().tokenizeString("ab\"cd\"");
    // ab then quoted cd
    EXPECT_EQ(2, veGetCmd().argc());
    EXPECT_STREQ("ab", veGetCmd().argv(0));
    EXPECT_STREQ("cd", veGetCmd().argv(1));
}

// ------------------------------- command buffer semantics --------------------------------
UTEST(veCmd_x, semicolon_inside_quotes_not_split) {
    resetVvarForTest();
    veCVar::get("v", "x", 0);
    veGetCmd().executeString("set v \"a;b\"");
    EXPECT_STREQ("a;b", veCVar::variableString("v"));
}

UTEST(veCmd_x, wait_defers_remaining_buffer) {
    resetVvarForTest();
    int count = 0;
    veGetCmd().addCommand("inc", [&count]() { count++; });
    // append then execute frames
    veGetCmd().execute(VE_CMD_EXEC_APPEND, "inc\nwait\ninc\n");
    veGetCmd().execute(VE_CMD_EXEC_NOW);
    EXPECT_EQ(1, count); // stopped at wait
    veGetCmd().execute(VE_CMD_EXEC_NOW);
    EXPECT_EQ(2, count); // second frame runs remainder
}

UTEST(veCmd_x, insert_prepends_to_buffer) {
    resetVvarForTest();
    std::string order;
    veGetCmd().addCommand("a", [&order]() { order += "a"; });
    veGetCmd().addCommand("b", [&order]() { order += "b"; });
    veGetCmd().execute(VE_CMD_EXEC_APPEND, "a\n");
    veGetCmd().execute(VE_CMD_EXEC_INSERT, "b");
    veGetCmd().execute(VE_CMD_EXEC_NOW);
    EXPECT_STREQ("ba", order.c_str());
}

// ------------------------------- aliases --------------------------------
UTEST(veCmd_x, alias_list_when_no_args) {
    resetVvarForTest();
    veGetCmd().executeString("alias myalias echo hi");
    vvarTestClearLog();
    veGetCmd().executeString("alias");
    EXPECT_TRUE(logContains("myalias"));
}

UTEST(veCmd_x, alias_redefine) {
    resetVvarForTest();
    veGetCmd().executeString("alias a echo first");
    veGetCmd().executeString("alias a echo second");
    vvarTestClearLog();
    veGetCmd().executeString("a");
    EXPECT_TRUE(logContains("second"));
    EXPECT_FALSE(logContains("first"));
}

UTEST(veCmd_x, alias_rejects_reserved_name) {
    resetVvarForTest();
    vvarTestClearLog();
    veGetCmd().executeString("alias alias foo");
    EXPECT_TRUE(logContains("invalid alias name"));
}

UTEST(veCmd_x, alias_query_nonexistent) {
    resetVvarForTest();
    vvarTestClearLog();
    veGetCmd().executeString("alias ghost");
    EXPECT_TRUE(logContains("does not exist"));
}

// ------------------------------- exec edge cases --------------------------------
UTEST(veCmd_x, exec_missing_file_reports) {
    resetVvarForTest();
    vvarTestClearLog();
    veGetCmd().executeString("exec definitely_missing_file_123.cfg");
    EXPECT_TRUE(logContains("couldn't exec"));
}

UTEST(veCmd_x, exec_wrong_args_usage) {
    resetVvarForTest();
    vvarTestClearLog();
    veGetCmd().executeString("exec");
    EXPECT_TRUE(logContains("usage: exec"));
}

// ------------------------------- toggle --------------------------------
UTEST(veCVarCmd_x, toggle_from_one_to_zero) {
    resetVvarForTest();
    veCVar::get("v", "1", 0);
    veGetCmd().executeString("toggle v");
    EXPECT_STREQ("0", veCVar::variableString("v"));
}

UTEST(veCVarCmd_x, toggle_cycles_value_list) {
    resetVvarForTest();
    veCVar::get("v", "a", 0);
    veGetCmd().executeString("toggle v a b c");
    EXPECT_STREQ("b", veCVar::variableString("v"));
    veGetCmd().executeString("toggle v a b c");
    EXPECT_STREQ("c", veCVar::variableString("v"));
    veGetCmd().executeString("toggle v a b c");
    EXPECT_STREQ("a", veCVar::variableString("v")); // wraps
}

UTEST(veCVarCmd_x, toggle_no_args_usage) {
    resetVvarForTest();
    vvarTestClearLog();
    veGetCmd().executeString("toggle");
    EXPECT_TRUE(logContains("usage: toggle"));
}

// ------------------------------- set variants no-arg printing --------------------------------
UTEST(veCVarCmd_x, set_one_arg_prints) {
    resetVvarForTest();
    veCVar::get("v", "val", 0);
    vvarTestClearLog();
    veGetCmd().executeString("set v");
    EXPECT_TRUE(logContains("v"));
    EXPECT_STREQ("val", veCVar::variableString("v"));
}

UTEST(veCVarCmd_x, set_no_args_usage) {
    resetVvarForTest();
    vvarTestClearLog();
    veGetCmd().executeString("set");
    EXPECT_TRUE(logContains("usage"));
}

UTEST(veCVarCmd_x, set_multi_token_value_joined) {
    resetVvarForTest();
    veGetCmd().executeString("set v hello world foo");
    EXPECT_STREQ("hello world foo", veCVar::variableString("v"));
}

UTEST(veCVarCmd_x, set_creates_cvar) {
    resetVvarForTest();
    veGetCmd().executeString("set newvar 99");
    EXPECT_EQ(99, veCVar::variableIntegerValue("newvar"));
}

// ------------------------------- print command --------------------------------
UTEST(veCVarCmd_x, print_nonexistent) {
    resetVvarForTest();
    vvarTestClearLog();
    veGetCmd().executeString("print ghostvar");
    EXPECT_TRUE(logContains("does not exist"));
}

// ------------------------------- cvar_modified --------------------------------
UTEST(veCVarCmd_x, cvar_modified_lists_changed) {
    resetVvarForTest();
    veCVar::get("changed", "d", 0);
    veCVar::set("changed", "m");
    vvarTestClearLog();
    veGetCmd().executeString("cvar_modified");
    EXPECT_TRUE(logContains("changed"));
}

// ------------------------------- filter deep --------------------------------
UTEST(veCmd_x, filter_char_range) {
    resetVvarForTest();
    EXPECT_TRUE(veCmd::filter("[a-c]at", "bat"));
    EXPECT_FALSE(veCmd::filter("[a-c]at", "zat"));
}

UTEST(veCmd_x, filter_leading_and_trailing_star) {
    resetVvarForTest();
    EXPECT_TRUE(veCmd::filter("*mid*", "xxmidyy"));
    EXPECT_FALSE(veCmd::filter("*mid*", "xxxyy"));
}

UTEST(veCmd_x, filter_exact_no_wildcard) {
    resetVvarForTest();
    EXPECT_TRUE(veCmd::filter("exact", "exact"));
    EXPECT_FALSE(veCmd::filter("exact", "exactly"));
}

UTEST(veCmd_x, stringContains_not_found) {
    resetVvarForTest();
    EXPECT_EQ(nullptr, veCmd::stringContains("hello", "xyz"));
}

// ------------------------------- veq3_va rotating buffers --------------------------------
UTEST(veq3_va_x, four_distinct_buffers) {
    resetVvarForTest();
    char* a = veq3_va("%d", 1);
    char* b = veq3_va("%d", 2);
    char* c = veq3_va("%d", 3);
    char* d = veq3_va("%d", 4);
    EXPECT_STREQ("1", a);
    EXPECT_STREQ("2", b);
    EXPECT_STREQ("3", c);
    EXPECT_STREQ("4", d);
    EXPECT_NE(a, b);
    EXPECT_NE(b, c);
}

UTEST(veq3_va_x, fifth_call_reuses_first_buffer) {
    resetVvarForTest();
    char* a = veq3_va("%d", 1);
    veq3_va("%d", 2);
    veq3_va("%d", 3);
    veq3_va("%d", 4);
    char* e = veq3_va("%d", 5);
    EXPECT_EQ(a, e);
}

// ------------------------------- misc integration --------------------------------
UTEST(veCmd_x, cvar_command_change_via_bareword) {
    resetVvarForTest();
    veCVar::get("v", "old", 0);
    veGetCmd().executeString("v newval");
    EXPECT_STREQ("newval", veCVar::variableString("v"));
}

UTEST(veCmd_x, alias_takes_priority_over_command) {
    resetVvarForTest();
    bool cmdCalled = false;
    veGetCmd().addCommand("shadow", [&cmdCalled]() { cmdCalled = true; });
    veGetCmd().executeString("alias shadow echo aliased");
    vvarTestClearLog();
    veGetCmd().executeString("shadow");
    EXPECT_TRUE(logContains("aliased"));
    EXPECT_FALSE(cmdCalled);
}

// =====================================================================================
// ============================ EXPANDED COVERAGE (batch 2) =============================
// =====================================================================================

// ------------------------------- numeric conversions --------------------------------
UTEST(veCVar_y, variableValue_nonnumeric_is_zero) {
    resetVvarForTest();
    veCVar::get("v", "hello", 0);
    EXPECT_NEAR(0.0f, veCVar::variableValue("v"), 0.001f);
    EXPECT_EQ(0, veCVar::variableIntegerValue("v"));
}

UTEST(veCVar_y, variableInteger_truncates_float_string) {
    resetVvarForTest();
    veCVar::get("v", "42.9", 0);
    EXPECT_EQ(42, veCVar::variableIntegerValue("v"));
    EXPECT_NEAR(42.9f, veCVar::variableValue("v"), 0.001f);
}

UTEST(veCVar_y, variableValue_nonexistent_zero) {
    resetVvarForTest();
    EXPECT_NEAR(0.0f, veCVar::variableValue("ghost"), 0.001f);
    EXPECT_EQ(0, veCVar::variableIntegerValue("ghost"));
}

UTEST(veCVar_y, setValue_integer_formatting) {
    resetVvarForTest();
    veCVar::get("v", "0", 0);
    veCVar::setValue("v", 7.0f);
    EXPECT_STREQ("7", veCVar::variableString("v"));
}

UTEST(veCVar_y, setValue_float_formatting) {
    resetVvarForTest();
    veCVar::get("v", "0", 0);
    veCVar::setValue("v", 1.5f);
    EXPECT_STREQ("1.500000", veCVar::variableString("v"));
}

UTEST(veCVar_y, setValueSafe_blocked_on_protected) {
    resetVvarForTest();
    veCVar::get("v", "0", VE_CVAR_PROTECTED);
    veCVar::setValueSafe("v", 5.0f);
    EXPECT_STREQ("0", veCVar::variableString("v"));
}

// ------------------------------- variableStringBuffer --------------------------------
UTEST(veCVar_y, variableStringBuffer_copies) {
    resetVvarForTest();
    veCVar::get("v", "buffered", 0);
    char buf[64];
    veCVar::variableStringBuffer("v", buf, sizeof(buf));
    EXPECT_STREQ("buffered", buf);
}

UTEST(veCVar_y, variableStringBuffer_nonexistent_empty) {
    resetVvarForTest();
    char buf[64];
    buf[0] = 'X';
    veCVar::variableStringBuffer("ghost", buf, sizeof(buf));
    EXPECT_STREQ("", buf);
}

// ------------------------------- name validation --------------------------------
UTEST(veCVar_y, name_with_semicolon_is_badname) {
    resetVvarForTest();
    veCVar* cv = veCVar::get("a;b", "v", 0);
    ASSERT_NE(cv, nullptr);
    EXPECT_STREQ("BADNAME", cv->getString().c_str());
}

UTEST(veCVar_y, name_with_quote_is_badname) {
    resetVvarForTest();
    veCVar* cv = veCVar::get("a\"b", "v", 0);
    ASSERT_NE(cv, nullptr);
    EXPECT_STREQ("BADNAME", cv->getString().c_str());
}

// ------------------------------- forceReset via reset semantics --------------------------------
UTEST(veCVar_y, reset_nonexistent_no_crash) {
    resetVvarForTest();
    veCVar::reset("ghost");
    SUCCEED();
}

// ------------------------------- list output flag letters --------------------------------
UTEST(veCVar_y, list_shows_archive_letter) {
    resetVvarForTest();
    veCVar::get("av", "1", VE_CVAR_ARCHIVE);
    vvarTestClearLog();
    veCVar::list();
    EXPECT_TRUE(logContains("av"));
    EXPECT_TRUE(logContains("total cvars"));
}

UTEST(veCVar_y, list_with_match_filter) {
    resetVvarForTest();
    veCVar::get("keepme", "1", 0);
    veCVar::get("other", "1", 0);
    vvarTestClearLog();
    veCVar::list("keep*");
    EXPECT_TRUE(logContains("keepme"));
    EXPECT_FALSE(logContains("other"));
}

// ------------------------------- commandCompletion (cvars) --------------------------------
UTEST(veCVar_y, commandCompletion_visits_cvars) {
    resetVvarForTest();
    veCVar::get("cc_test", "1", 0);
    bool seen = false;
    veCVar::commandCompletion([&seen](const char* s) {
        if (std::string(s) == "cc_test") seen = true;
    });
    EXPECT_TRUE(seen);
}

// ------------------------------- cmd registry helpers --------------------------------
UTEST(veCmd_y, addCommand_duplicate_warns) {
    resetVvarForTest();
    veGetCmd().addCommand("dupwarn", [](){});
    vvarTestClearLog();
    veGetCmd().addCommand("dupwarn", [](){});
    EXPECT_TRUE(logContains("already defined"));
}

UTEST(veCmd_y, removeCommand_nonexistent_no_crash) {
    resetVvarForTest();
    veGetCmd().removeCommand("never_existed");
    SUCCEED();
}

UTEST(veCmd_y, getArgvIdx_and_len) {
    resetVvarForTest();
    veGetCmd().tokenizeString("aa bbb");
    EXPECT_EQ(0, veGetCmd().getArgvIdx(0));
    EXPECT_EQ(-1, veGetCmd().getArgvIdx(9));
    EXPECT_EQ(2, veGetCmd().getArgvLen(0));
    EXPECT_EQ(3, veGetCmd().getArgvLen(1));
}

// ------------------------------- cmdlist filter --------------------------------
UTEST(veCmd_y, cmdlist_filter_match) {
    resetVvarForTest();
    veGetCmd().addCommand("zzspecial", [](){});
    vvarTestClearLog();
    veGetCmd().executeString("cmdlist zz*");
    EXPECT_TRUE(logContains("zzspecial"));
}

// ------------------------------- multiple waits + buffering --------------------------------
UTEST(veCmd_y, two_waits_delay_two_frames) {
    resetVvarForTest();
    int count = 0;
    veGetCmd().addCommand("inc", [&count]() { count++; });
    veGetCmd().execute(VE_CMD_EXEC_APPEND, "inc\nwait\nwait\ninc\n");
    veGetCmd().execute(VE_CMD_EXEC_NOW);
    EXPECT_EQ(1, count);
    veGetCmd().execute(VE_CMD_EXEC_NOW);
    EXPECT_EQ(1, count); // second wait still pending
    veGetCmd().execute(VE_CMD_EXEC_NOW);
    EXPECT_EQ(2, count);
}

UTEST(veCmd_y, wait_negative_arg_becomes_one) {
    resetVvarForTest();
    veGetCmd().executeString("wait -5");
    EXPECT_EQ(1, veGetCmd().getWait());
}

// ------------------------------- exec nested --------------------------------
UTEST(veCmd_y, exec_nested_files) {
    resetVvarForTest();
    {
        std::ofstream a("vvar_nested_a.cfg", std::ios::trunc);
        a << "exec vvar_nested_b.cfg\nset from_a 1\n";
    }
    {
        std::ofstream b("vvar_nested_b.cfg", std::ios::trunc);
        b << "set from_b 2\n";
    }
    veGetCmd().executeString("exec vvar_nested_a.cfg");
    EXPECT_EQ(1, veCVar::variableIntegerValue("from_a"));
    EXPECT_EQ(2, veCVar::variableIntegerValue("from_b"));
    std::remove("vvar_nested_a.cfg");
    std::remove("vvar_nested_b.cfg");
}

// ------------------------------- info string with spaces --------------------------------
UTEST(veIVar_y, value_with_spaces_roundtrip) {
    resetVvarForTest();
    veIVar::set("s", "motd", "hello world");
    std::string out = veIVar::toString("s");
    EXPECT_TRUE(out.find("\\motd\\hello world") != std::string::npos);
    veIVar::fromString("s2", out.c_str());
    EXPECT_STREQ("hello world", veIVar::get("s2", "motd"));
}

// ------------------------------- executeString leading newline / crlf --------------------------------
UTEST(veCmd_y, executeString_crlf_lines) {
    resetVvarForTest();
    int count = 0;
    veGetCmd().addCommand("inc", [&count]() { count++; });
    veGetCmd().executeString("inc\r\ninc\r\n");
    EXPECT_EQ(2, count);
}

// ------------------------------- toggle nothing-to-toggle-to --------------------------------
UTEST(veCVarCmd_y, toggle_single_value_arg_reports) {
    resetVvarForTest();
    veCVar::get("v", "a", 0);
    vvarTestClearLog();
    veGetCmd().executeString("toggle v onlyone");
    EXPECT_TRUE(logContains("nothing to toggle"));
}

// ------------------------------- reset command usage --------------------------------
UTEST(veCVarCmd_y, reset_wrong_args_usage) {
    resetVvarForTest();
    vvarTestClearLog();
    veGetCmd().executeString("reset");
    EXPECT_TRUE(logContains("usage: reset"));
}

// =====================================================================================
// ======================= Compatibility behavior regression tests =======================
// =====================================================================================

UTEST(compatibility, exec_usage_includes_filename_placeholder) {
    resetVvarForTest();
    vvarTestClearLog();
    veGetCmd().executeString("exec");
    EXPECT_TRUE(logContains("usage: exec <filename>"));
}

UTEST(compatibility, cvarlist_filtered_output_total_counts_all_cvars) {
    resetVvarForTest();
    veCVar::get("mini_keep", "1", 0);
    veCVar::get("mini_other", "2", 0);
    vvarTestClearLog();
    veGetCmd().executeString("cvarlist mini_keep");
    EXPECT_TRUE(logContains("mini_keep"));
    EXPECT_FALSE(logContains("mini_other \"2\""));
    // Total reports all registered cvars, not only filtered rows. Includes sv_cheats.
    EXPECT_TRUE(logContains("3 total cvars"));
}

UTEST(compatibility, cvar_modified_filtered_total_counts_all_modified_cvars) {
    resetVvarForTest();
    veCVar::get("mini_mod_keep", "0", 0);
    veCVar::get("mini_mod_other", "0", 0);
    veCVar::set("mini_mod_keep", "1");
    veCVar::set("mini_mod_other", "1");
    vvarTestClearLog();
    veGetCmd().executeString("cvar_modified mini_mod_keep");
    EXPECT_TRUE(logContains("mini_mod_keep"));
    EXPECT_FALSE(logContains("mini_mod_other \"1\""));
    // Total reports all modified cvars, not only filtered rows.
    EXPECT_TRUE(logContains("2 total modified cvars"));
}

UTEST(compatibility, command_buffer_semicolon_inside_single_quotes_still_splits) {
    resetVvarForTest();
    vvarTestClearLog();
    veGetCmd().executeString("echo 'a;b'");
    EXPECT_TRUE(logContains("a\n"));
    EXPECT_FALSE(logContains("a;b\n"));
}

UTEST_MAIN()
