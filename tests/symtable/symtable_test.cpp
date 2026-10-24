// Implementation of the IFJ26 imperative language compiler
// symtable_test.cpp
// Author(s): Adam Zatloukal (xzatloa01)
//
// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Create a test usinsg google-tests for push and pop scope
//          where they return ST_OK and symtable depth can range from 0 to 5.
//          Also try to pop scope on an empty stack and expect ST_INVALID, and
//          make sure you cannot access the variables after pop_scope"

extern "C" {
#include "symtable.h"
}

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <set>
#include <string>
#include <vector>

// Fresh symbol table for every test, freed afterwards
class SymtableScopeTest : public ::testing::Test {
protected:
    symtable_t* st = nullptr;

    void SetUp() override
    {
        st = symtable_new();
        ASSERT_NE(st, nullptr);
    }

    void TearDown() override
    {
        symtable_free(st);
    }
};

// A new symtable has no scope pushed, so its depth is 0 (global)
TEST_F(SymtableScopeTest, NewTableHasDepthZero)
{
    EXPECT_EQ(symtable_depth(st), 0);
}

// Pushing 5 scopes raises the depth 1..5, popping them lowers it back to 0
TEST_F(SymtableScopeTest, PushAndPopChangeDepthFromZeroToFive)
{
    for (int depth = 1; depth <= 5; depth++) {
        ASSERT_EQ(symtable_push_scope(st), ST_OK);
        EXPECT_EQ(symtable_depth(st), depth);
    }

    for (int depth = 4; depth >= 0; depth--) {
        ASSERT_EQ(symtable_pop_scope(st), ST_OK);
        EXPECT_EQ(symtable_depth(st), depth);
    }
}

// Popping with no scope on the stack is invalid and leaves the depth at 0
TEST_F(SymtableScopeTest, PopOnEmptyStackIsInvalid)
{
    EXPECT_EQ(symtable_pop_scope(st), ST_INVALID);
    EXPECT_EQ(symtable_depth(st), 0);
}

// Popping every pushed scope and then once more is also invalid
TEST_F(SymtableScopeTest, PopAfterAllScopesPoppedIsInvalid)
{
    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    ASSERT_EQ(symtable_pop_scope(st), ST_OK);
    EXPECT_EQ(symtable_pop_scope(st), ST_INVALID);
}

// NULL handle is rejected by both operations
TEST_F(SymtableScopeTest, NullHandleIsInvalid)
{
    EXPECT_EQ(symtable_push_scope(nullptr), ST_INVALID);
    EXPECT_EQ(symtable_pop_scope(nullptr), ST_INVALID);
}

// A variable from a popped scope cannot be found anymore
TEST_F(SymtableScopeTest, VariableNotAccessibleAfterPop)
{
    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    ASSERT_EQ(symtable_insert_var(st, "x", TYPE_INT, false, nullptr), ST_OK);
    ASSERT_NE(symtable_lookup_var(st, "x"), nullptr);

    ASSERT_EQ(symtable_pop_scope(st), ST_OK);
    EXPECT_EQ(symtable_lookup_var(st, "x"), nullptr);
    EXPECT_EQ(symtable_lookup_local(st, "x"), nullptr);
}

// Popping an inner scope removes only its variables, outer ones stay visible
TEST_F(SymtableScopeTest, PopRemovesOnlyInnerScopeVariables)
{
    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    ASSERT_EQ(symtable_insert_var(st, "outer", TYPE_INT, false, nullptr), ST_OK);

    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    ASSERT_EQ(symtable_insert_var(st, "inner", TYPE_STRING, true, nullptr), ST_OK);
    ASSERT_NE(symtable_lookup_var(st, "inner"), nullptr);
    ASSERT_NE(symtable_lookup_var(st, "outer"), nullptr);

    ASSERT_EQ(symtable_pop_scope(st), ST_OK);
    EXPECT_EQ(symtable_lookup_var(st, "inner"), nullptr);
    EXPECT_NE(symtable_lookup_var(st, "outer"), nullptr);
}

// After popping a shadowing scope, the name resolves to the outer variable again
TEST_F(SymtableScopeTest, ShadowedVariableRestoredAfterPop)
{
    symbol_t* outer = nullptr;
    symbol_t* inner = nullptr;

    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    ASSERT_EQ(symtable_insert_var(st, "x", TYPE_INT, false, &outer), ST_OK);

    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    ASSERT_EQ(symtable_insert_var(st, "x", TYPE_DOUBLE, true, &inner), ST_OK);
    EXPECT_EQ(symtable_lookup_var(st, "x"), inner);

    ASSERT_EQ(symtable_pop_scope(st), ST_OK);
    symbol_t* found = symtable_lookup_var(st, "x");
    EXPECT_EQ(found, outer);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->type, TYPE_INT);
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Now add a test for: inserting var onto the same scope twice, check
//          that ST_EXISTS is returned and out points to the original. Add a
//          shadowing case where same name is in 2 scopes and check that at
//          first it returns the one from the top then pop and make sure it
//          returns the var from that scope. lookup finds a var in a scope that
//          isnt on the top and lookup local doesnt find the same var. inserting
//          without any scope. lookup_var_ex returns -1 when name isnt found.
//          is_const is stored correctly for val and var"

// Same fixture, separate suite name for the variable tests
class SymtableVarTest : public SymtableScopeTest { };

// Second insert of the same name into one scope fails and points to the original
TEST_F(SymtableVarTest, DuplicateInSameScopeReturnsExisting)
{
    symbol_t* original = nullptr;
    symbol_t* duplicate = nullptr;

    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    ASSERT_EQ(symtable_insert_var(st, "x", TYPE_INT, false, &original), ST_OK);
    ASSERT_NE(original, nullptr);

    EXPECT_EQ(symtable_insert_var(st, "x", TYPE_STRING, true, &duplicate), ST_EXISTS);
    EXPECT_EQ(duplicate, original);

    // The original symbol must not be overwritten by the failed insert
    EXPECT_EQ(original->type, TYPE_INT);
    EXPECT_FALSE(original->is_const);
}

// Shadowing: the top scope's variable wins, after pop the lower one is found again
TEST_F(SymtableVarTest, ShadowingResolvesToTopScopeThenLowerAfterPop)
{
    symbol_t* lower = nullptr;
    symbol_t* top = nullptr;
    int depth = 0;

    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    ASSERT_EQ(symtable_insert_var(st, "x", TYPE_INT, false, &lower), ST_OK);
    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    ASSERT_EQ(symtable_insert_var(st, "x", TYPE_DOUBLE, true, &top), ST_OK);
    ASSERT_NE(lower, top);

    EXPECT_EQ(symtable_lookup_var_ex(st, "x", &depth), top);
    EXPECT_EQ(depth, 2);

    ASSERT_EQ(symtable_pop_scope(st), ST_OK);
    EXPECT_EQ(symtable_lookup_var_ex(st, "x", &depth), lower);
    EXPECT_EQ(depth, 1);
}

// lookup_var searches outer scopes, lookup_local only the top scope
TEST_F(SymtableVarTest, OuterVarFoundByLookupButNotByLookupLocal)
{
    symbol_t* outer = nullptr;

    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    ASSERT_EQ(symtable_insert_var(st, "x", TYPE_INT, false, &outer), ST_OK);
    ASSERT_EQ(symtable_push_scope(st), ST_OK);

    EXPECT_EQ(symtable_lookup_var(st, "x"), outer);
    EXPECT_EQ(symtable_lookup_local(st, "x"), nullptr);
}

// Without a pushed scope there is nowhere to insert a variable
TEST_F(SymtableVarTest, InsertWithoutScopeIsInvalid)
{
    symbol_t* out = reinterpret_cast<symbol_t*>(0x1); // must be reset to NULL

    EXPECT_EQ(symtable_insert_var(st, "x", TYPE_INT, false, &out), ST_INVALID);
    EXPECT_EQ(out, nullptr);
    EXPECT_EQ(symtable_lookup_var(st, "x"), nullptr);
}

// lookup_var_ex reports depth -1 for a name that is not in any scope
TEST_F(SymtableVarTest, LookupVarExMissingNameGivesMinusOne)
{
    int depth = 42;

    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    ASSERT_EQ(symtable_insert_var(st, "x", TYPE_INT, false, nullptr), ST_OK);
    ASSERT_EQ(symtable_push_scope(st), ST_OK);

    EXPECT_EQ(symtable_lookup_var_ex(st, "missing", &depth), nullptr);
    EXPECT_EQ(depth, -1);
}

// is_const is true for val and false for var
TEST_F(SymtableVarTest, IsConstStoredForValAndVar)
{
    symbol_t* val_sym = nullptr;
    symbol_t* var_sym = nullptr;

    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    ASSERT_EQ(symtable_insert_var(st, "a", TYPE_INT, true, &val_sym), ST_OK);
    ASSERT_EQ(symtable_insert_var(st, "b", TYPE_INT, false, &var_sym), ST_OK);

    ASSERT_NE(val_sym, nullptr);
    ASSERT_NE(var_sym, nullptr);
    EXPECT_TRUE(val_sym->is_const);
    EXPECT_FALSE(var_sym->is_const);

    // The stored flag is what a later lookup sees too
    EXPECT_TRUE(symtable_lookup_var(st, "a")->is_const);
    EXPECT_FALSE(symtable_lookup_var(st, "b")->is_const);
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "create a test where bst_insert sets res to ST_NOMEM (node allocation
//          fails) and test that insert_var catches the error"

// The test binary is linked with -Wl,--wrap=calloc, so every calloc call goes
// through __wrap_calloc. While fail_next_calloc is set, the next call returns
// NULL (that is the node allocation in allocate_node), otherwise it forwards
// to the real calloc.
static bool fail_next_calloc = false;

extern "C" void* __real_calloc(size_t count, size_t size);

extern "C" void* __wrap_calloc(size_t count, size_t size)
{
    if (fail_next_calloc) {
        fail_next_calloc = false;
        return nullptr;
    }
    return __real_calloc(count, size);
}

class SymtableNoMemTest : public SymtableScopeTest {
protected:
    void TearDown() override
    {
        fail_next_calloc = false; // never leak the failure into other tests
        SymtableScopeTest::TearDown();
    }
};

// Node allocation fails on an empty tree: ST_NOMEM, out stays NULL, nothing inserted
TEST_F(SymtableNoMemTest, InsertVarIntoEmptyScopeReportsNoMem)
{
    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    symbol_t* out = reinterpret_cast<symbol_t*>(0x1); // must be reset to NULL

    fail_next_calloc = true;
    EXPECT_EQ(symtable_insert_var(st, "x", TYPE_INT, false, &out), ST_NOMEM);
    EXPECT_FALSE(fail_next_calloc); // the failing calloc was really reached
    EXPECT_EQ(out, nullptr);
    EXPECT_EQ(symtable_lookup_var(st, "x"), nullptr);

    // The scope is still usable afterwards
    EXPECT_EQ(symtable_insert_var(st, "x", TYPE_INT, false, &out), ST_OK);
    EXPECT_NE(out, nullptr);
}

// Node allocation fails deep in a non-empty tree (root stays non-NULL), the
// error must still be reported and the existing variables must be untouched
TEST_F(SymtableNoMemTest, InsertVarIntoNonEmptyScopeReportsNoMem)
{
    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    const char* names[] = { "d", "b", "f", "a", "c", "e", "g" };
    symbol_t* existing[7];
    for (int i = 0; i < 7; i++)
        ASSERT_EQ(symtable_insert_var(st, names[i], TYPE_INT, false, &existing[i]), ST_OK);

    symbol_t* out = reinterpret_cast<symbol_t*>(0x1);
    fail_next_calloc = true;
    EXPECT_EQ(symtable_insert_var(st, "h", TYPE_INT, false, &out), ST_NOMEM);
    EXPECT_FALSE(fail_next_calloc);
    EXPECT_EQ(out, nullptr);
    EXPECT_EQ(symtable_lookup_var(st, "h"), nullptr);

    for (int i = 0; i < 7; i++)
        EXPECT_EQ(symtable_lookup_var(st, names[i]), existing[i]);
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Now add tests for functions: insert, lookup the same name twice and
//          check if ST_EXISTS you can insert without a pushed scope and that
//          the global scope is visible when stack pointer is NULL. Function and
//          var with the same name doesnt break anything. add_param, count order
//          and types are appened correctly and that a duplicate param returns
//          ST_EXISTS and NULL retuns ST_INVALID"

// Same fixture, separate suite name for the function tests
class SymtableFuncTest : public SymtableScopeTest { };

// Insert, look up, then a second insert of the same name fails with ST_EXISTS
TEST_F(SymtableFuncTest, InsertLookupAndDuplicateReturnsExisting)
{
    symbol_t* original = nullptr;
    symbol_t* duplicate = nullptr;

    ASSERT_EQ(symtable_insert_func(st, "foo", &original), ST_OK);
    ASSERT_NE(original, nullptr);
    EXPECT_EQ(original->kind, SYM_FUNC);
    EXPECT_EQ(symtable_lookup_func(st, "foo"), original);

    EXPECT_EQ(symtable_insert_func(st, "foo", &duplicate), ST_EXISTS);
    EXPECT_EQ(duplicate, original);
    EXPECT_EQ(symtable_lookup_func(st, "foo"), original);
}

// Functions do not need a scope: insert and lookup work with an empty stack
TEST_F(SymtableFuncTest, InsertAndLookupWithoutPushedScope)
{
    symbol_t* func = nullptr;

    ASSERT_EQ(symtable_depth(st), 0);
    ASSERT_EQ(symtable_insert_func(st, "foo", &func), ST_OK);
    EXPECT_EQ(symtable_lookup_func(st, "foo"), func);
    EXPECT_EQ(symtable_lookup_func(st, "missing"), nullptr);
}

// The global function table stays visible inside scopes and after they are popped
TEST_F(SymtableFuncTest, GlobalVisibleAcrossPushAndPop)
{
    symbol_t* func = nullptr;
    ASSERT_EQ(symtable_insert_func(st, "foo", &func), ST_OK);

    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    EXPECT_EQ(symtable_lookup_func(st, "foo"), func);

    ASSERT_EQ(symtable_pop_scope(st), ST_OK);
    ASSERT_EQ(symtable_pop_scope(st), ST_OK);
    ASSERT_EQ(symtable_depth(st), 0); // stack pointer is NULL again
    EXPECT_EQ(symtable_lookup_func(st, "foo"), func);
}

// A function and a variable with the same name live in separate tables
TEST_F(SymtableFuncTest, FunctionAndVariableWithSameName)
{
    symbol_t* func = nullptr;
    symbol_t* var = nullptr;

    ASSERT_EQ(symtable_insert_func(st, "x", &func), ST_OK);
    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    ASSERT_EQ(symtable_insert_var(st, "x", TYPE_INT, false, &var), ST_OK);
    ASSERT_NE(func, var);

    EXPECT_EQ(symtable_lookup_func(st, "x"), func);
    EXPECT_EQ(symtable_lookup_var(st, "x"), var);
    EXPECT_EQ(func->kind, SYM_FUNC);
    EXPECT_EQ(var->kind, SYM_VAR);

    // Removing the variable's scope must not affect the function
    ASSERT_EQ(symtable_pop_scope(st), ST_OK);
    EXPECT_EQ(symtable_lookup_func(st, "x"), func);
}

// Parameters are appended in order with the right names, types and count
TEST_F(SymtableFuncTest, AddParamAppendsInOrder)
{
    symbol_t* func = nullptr;
    ASSERT_EQ(symtable_insert_func(st, "subStr", &func), ST_OK);
    EXPECT_EQ(func->param_count, 0);

    ASSERT_EQ(symtable_func_add_param(func, "value", TYPE_STRING), ST_OK);
    ASSERT_EQ(symtable_func_add_param(func, "start", TYPE_INT), ST_OK);
    ASSERT_EQ(symtable_func_add_param(func, "end", TYPE_INT), ST_OK);

    ASSERT_EQ(func->param_count, 3);
    EXPECT_STREQ(func->param_names[0], "value");
    EXPECT_STREQ(func->param_names[1], "start");
    EXPECT_STREQ(func->param_names[2], "end");
    EXPECT_EQ(func->param_types[0], TYPE_STRING);
    EXPECT_EQ(func->param_types[1], TYPE_INT);
    EXPECT_EQ(func->param_types[2], TYPE_INT);

    // The same data is seen through a later lookup
    EXPECT_EQ(symtable_lookup_func(st, "subStr")->param_count, 3);
}

// The parameter name is copied, so the caller's buffer can be reused
TEST_F(SymtableFuncTest, AddParamCopiesName)
{
    symbol_t* func = nullptr;
    char buffer[] = "a";

    ASSERT_EQ(symtable_insert_func(st, "foo", &func), ST_OK);
    ASSERT_EQ(symtable_func_add_param(func, buffer, TYPE_INT), ST_OK);
    buffer[0] = 'z';

    EXPECT_STREQ(func->param_names[0], "a");
}

// A duplicate parameter name is rejected and nothing is appended
TEST_F(SymtableFuncTest, AddParamDuplicateReturnsExisting)
{
    symbol_t* func = nullptr;
    ASSERT_EQ(symtable_insert_func(st, "foo", &func), ST_OK);

    ASSERT_EQ(symtable_func_add_param(func, "a", TYPE_INT), ST_OK);
    EXPECT_EQ(symtable_func_add_param(func, "a", TYPE_STRING), ST_EXISTS);

    ASSERT_EQ(func->param_count, 1);
    EXPECT_EQ(func->param_types[0], TYPE_INT);
}

// NULL function, name or type is rejected and nothing is appended
TEST_F(SymtableFuncTest, AddParamNullArgumentsAreInvalid)
{
    symbol_t* func = nullptr;
    ASSERT_EQ(symtable_insert_func(st, "foo", &func), ST_OK);

    EXPECT_EQ(symtable_func_add_param(nullptr, "a", TYPE_INT), ST_INVALID);
    EXPECT_EQ(symtable_func_add_param(func, nullptr, TYPE_INT), ST_INVALID);
    EXPECT_EQ(symtable_func_add_param(func, "a", nullptr), ST_INVALID);

    EXPECT_EQ(func->param_count, 0);
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Now add benchmark tests: insert hunderds/thausands of names in random
//          order check if they can all be found and of they are correctly
//          inserted into the tree and that the tree is balanced. Ascending order
//          is the case that breaks an unbalanced tree. Add a case where every
//          symbol pointer returend from eralier inserts still points to the
//          correct data so that we know the rotations dont break anything. Also
//          add a case where you can foreach_func and make sure it runs on every
//          node ONCE in the correct order (in order). Collect the names using ctx"
//
// The tree is private to symtable.c, so balance is checked indirectly: an
// unbalanced BST degrades to a linked list on sorted input, which makes the
// large ascending/descending tests take O(n^2) time (and the recursive insert
// overflow the stack), so they fail on the time limit instead of passing.

namespace {

// Same fixture, separate suite name for the large tree tests
class SymtableTreeTest : public SymtableScopeTest { };

// Names "n000000", "n000001", ... sort by strcmp in the same order as their number
std::vector<std::string> make_names(int count)
{
    std::vector<std::string> names;
    names.reserve(count);
    for (int i = 0; i < count; i++) {
        char buf[16];
        std::snprintf(buf, sizeof buf, "n%06d", i);
        names.emplace_back(buf);
    }
    return names;
}

// Fixed seed, so a failing order can be reproduced
std::vector<std::string> shuffled(std::vector<std::string> names)
{
    std::mt19937 rng(42);
    std::shuffle(names.begin(), names.end(), rng);
    return names;
}

// Distinct data per index, used to check that symbols keep their own data
type_t* type_for(int i)
{
    type_t* types[] = { TYPE_INT, TYPE_DOUBLE, TYPE_STRING };
    return types[i % 3];
}

// Inserts every name as a variable into the top scope, then checks they are all found
void insert_and_find_all(symtable_t* st, const std::vector<std::string>& names)
{
    for (const std::string& name : names)
        ASSERT_EQ(symtable_insert_var(st, name.c_str(), TYPE_INT, false, nullptr), ST_OK) << name;

    for (const std::string& name : names) {
        symbol_t* sym = symtable_lookup_local(st, name.c_str());
        ASSERT_NE(sym, nullptr) << name;
        EXPECT_EQ(sym->kind, SYM_VAR);
    }

    // A second insert of any name must still see it as present
    for (const std::string& name : names)
        ASSERT_EQ(symtable_insert_var(st, name.c_str(), TYPE_INT, false, nullptr), ST_EXISTS) << name;
}

// Collects names visited by foreach into the vector passed as ctx
void collect_name(const char* name, symbol_t* sym, void* ctx)
{
    (void)sym;
    static_cast<std::vector<std::string>*>(ctx)->emplace_back(name);
}

} // namespace

// Thousands of names in random order are all found afterwards
TEST_F(SymtableTreeTest, RandomOrderAllFound)
{
    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    insert_and_find_all(st, shuffled(make_names(5000)));
    EXPECT_EQ(symtable_lookup_local(st, "missing"), nullptr);
}

// Sorted input is the worst case for an unbalanced BST; with AVL it stays fast
TEST_F(SymtableTreeTest, AscendingOrderLargeIsFast)
{
    const auto start = std::chrono::steady_clock::now();

    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    insert_and_find_all(st, make_names(100000));

    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start)
                        .count();
    RecordProperty("ascending_100k_ms", static_cast<int>(ms));
    // Balanced: well under a second even with sanitizers. Unbalanced: minutes.
    EXPECT_LT(ms, 5000) << "tree is probably not balanced";
}

// Descending input is the mirrored worst case
TEST_F(SymtableTreeTest, DescendingOrderLargeIsFast)
{
    std::vector<std::string> names = make_names(100000);
    std::reverse(names.begin(), names.end());

    const auto start = std::chrono::steady_clock::now();

    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    insert_and_find_all(st, names);

    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start)
                        .count();
    RecordProperty("descending_100k_ms", static_cast<int>(ms));
    EXPECT_LT(ms, 5000) << "tree is probably not balanced";
}

// Pointers returned by earlier inserts still point to their own data after
// thousands of later inserts (and the rotations they cause)
TEST_F(SymtableTreeTest, SymbolPointersStableAcrossRotations)
{
    const std::vector<std::string> names = make_names(5000);
    std::vector<symbol_t*> returned(names.size());

    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    // Ascending order forces a rotation on almost every insert
    for (size_t i = 0; i < names.size(); i++) {
        ASSERT_EQ(symtable_insert_var(st, names[i].c_str(), type_for(i), i % 2 == 0, &returned[i]), ST_OK);
        ASSERT_NE(returned[i], nullptr);
    }

    std::set<int> ids;
    for (size_t i = 0; i < names.size(); i++) {
        symbol_t* sym = returned[i];
        EXPECT_EQ(symtable_lookup_local(st, names[i].c_str()), sym) << names[i];
        EXPECT_EQ(sym->kind, SYM_VAR) << names[i];
        EXPECT_EQ(sym->type, type_for(i)) << names[i];
        EXPECT_EQ(sym->is_const, i % 2 == 0) << names[i];
        ids.insert(sym->id);
    }
    // Every symbol still has its own id, none were swapped or duplicated
    EXPECT_EQ(ids.size(), names.size());
}

// The same for function symbols in the global tree, including their parameters
TEST_F(SymtableTreeTest, FunctionPointersStableAcrossRotations)
{
    const std::vector<std::string> names = shuffled(make_names(2000));
    std::vector<symbol_t*> returned(names.size());

    for (size_t i = 0; i < names.size(); i++) {
        ASSERT_EQ(symtable_insert_func(st, names[i].c_str(), &returned[i]), ST_OK);
        // i % 4 parameters, so every function has a recognisable signature
        for (size_t p = 0; p < i % 4; p++) {
            std::string param = "p" + std::to_string(p);
            ASSERT_EQ(symtable_func_add_param(returned[i], param.c_str(), type_for(p)), ST_OK);
        }
    }

    for (size_t i = 0; i < names.size(); i++) {
        symbol_t* sym = returned[i];
        ASSERT_EQ(symtable_lookup_func(st, names[i].c_str()), sym) << names[i];
        ASSERT_EQ(sym->param_count, static_cast<int>(i % 4)) << names[i];
        for (int p = 0; p < sym->param_count; p++) {
            EXPECT_EQ(sym->param_names[p], "p" + std::to_string(p)) << names[i];
            EXPECT_EQ(sym->param_types[p], type_for(p)) << names[i];
        }
    }
}

// foreach_func visits every function exactly once, in sorted (in-order) order
TEST_F(SymtableTreeTest, ForeachFuncVisitsEachOnceInOrder)
{
    const std::vector<std::string> sorted = make_names(3000);
    for (const std::string& name : shuffled(sorted))
        ASSERT_EQ(symtable_insert_func(st, name.c_str(), nullptr), ST_OK);

    std::vector<std::string> visited;
    symtable_foreach_func(st, collect_name, &visited);

    // Same size + same order as the sorted list = each node once, in order
    ASSERT_EQ(visited.size(), sorted.size());
    EXPECT_EQ(visited, sorted);
}

// foreach_func on an empty table never calls the callback
TEST_F(SymtableTreeTest, ForeachFuncOnEmptyTable)
{
    std::vector<std::string> visited;
    symtable_foreach_func(st, collect_name, &visited);
    EXPECT_TRUE(visited.empty());
}

// AI-generated using: Claude Code
//              model: Claude Opus 5.5
// Prompt: "Now add test cases for types and ids: make sure every symbol has a
//          unique id, make sure type compatibility works."

// Same fixture, separate suite name for the id tests
class SymtableIdTest : public SymtableScopeTest { };

// Variables and functions share one id counter, so no two symbols collide
TEST_F(SymtableIdTest, VariablesAndFunctionsHaveUniqueIds)
{
    std::set<int> ids;
    symbol_t* sym = nullptr;

    for (int i = 0; i < 50; i++) {
        std::string name = "f" + std::to_string(i);
        ASSERT_EQ(symtable_insert_func(st, name.c_str(), &sym), ST_OK);
        EXPECT_TRUE(ids.insert(sym->id).second) << "duplicate id for " << name;
    }

    for (int depth = 0; depth < 5; depth++) {
        ASSERT_EQ(symtable_push_scope(st), ST_OK);
        for (int i = 0; i < 20; i++) {
            std::string name = "v" + std::to_string(i);
            ASSERT_EQ(symtable_insert_var(st, name.c_str(), TYPE_INT, false, &sym), ST_OK);
            EXPECT_TRUE(ids.insert(sym->id).second) << "duplicate id for " << name;
        }
    }

    EXPECT_EQ(ids.size(), 50u + 5u * 20u);
}

// Shadowing variables have the same name but must get different ids, which is
// what codegen uses to give them distinct IFJcode26 names
TEST_F(SymtableIdTest, ShadowedVariablesHaveDifferentIds)
{
    symbol_t* outer = nullptr;
    symbol_t* inner = nullptr;

    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    ASSERT_EQ(symtable_insert_var(st, "x", TYPE_INT, false, &outer), ST_OK);
    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    ASSERT_EQ(symtable_insert_var(st, "x", TYPE_INT, false, &inner), ST_OK);

    EXPECT_NE(outer->id, inner->id);
}

// Ids of popped symbols are not reused: a variable with the same name in a new
// sibling scope (e.g. the else branch) still gets a fresh id
TEST_F(SymtableIdTest, IdsNotReusedAfterPop)
{
    symbol_t* sym = nullptr;
    std::set<int> ids;

    for (int branch = 0; branch < 10; branch++) {
        ASSERT_EQ(symtable_push_scope(st), ST_OK);
        ASSERT_EQ(symtable_insert_var(st, "x", TYPE_INT, false, &sym), ST_OK);
        EXPECT_TRUE(ids.insert(sym->id).second) << "id reused in branch " << branch;
        ASSERT_EQ(symtable_pop_scope(st), ST_OK);
    }
}

// A symbol's id does not change when the tree is rebalanced
TEST_F(SymtableIdTest, IdStableAfterRebalance)
{
    symbol_t* first = nullptr;

    ASSERT_EQ(symtable_push_scope(st), ST_OK);
    ASSERT_EQ(symtable_insert_var(st, "a", TYPE_INT, false, &first), ST_OK);
    const int id = first->id;

    // Enough inserts to rotate the node holding "a"
    for (int i = 0; i < 100; i++) {
        std::string name = "b" + std::to_string(i);
        ASSERT_EQ(symtable_insert_var(st, name.c_str(), TYPE_INT, false, nullptr), ST_OK);
    }

    EXPECT_EQ(symtable_lookup_var(st, "a")->id, id);
}

namespace {

// All basic types usable in IFJ26, with readable names for failure messages
struct NamedType {
    const char* name;
    type_t* type;
};

std::vector<NamedType> basic_types()
{
    return {
        { "Int", TYPE_INT },
        { "Double", TYPE_DOUBLE },
        { "String", TYPE_STRING },
        { "Unit", TYPE_UNIT },
        { "Boolean", TYPE_BOOL },
        { "Any", TYPE_ANY },
    };
}

} // namespace

// type_equal is true only for the same kind
TEST(TypeTest, EqualOnlyForSameKind)
{
    for (const NamedType& a : basic_types()) {
        for (const NamedType& b : basic_types()) {
            EXPECT_EQ(type_equal(a.type, b.type), a.type == b.type)
                << a.name << " vs " << b.name;
        }
    }
}

// Two separate type_t values of the same kind are equal (compared by kind,
// not by pointer), which matters once composite types are allocated
TEST(TypeTest, EqualComparesKindNotPointer)
{
    type_t a = { T_INT };
    type_t b = { T_INT };
    EXPECT_TRUE(type_equal(&a, &b));
}

TEST(TypeTest, EqualWithNullIsFalse)
{
    EXPECT_FALSE(type_equal(nullptr, TYPE_INT));
    EXPECT_FALSE(type_equal(TYPE_INT, nullptr));
    EXPECT_FALSE(type_equal(nullptr, nullptr));
}

// Every type is compatible with itself
TEST(TypeTest, CompatibleWithItself)
{
    for (const NamedType& t : basic_types())
        EXPECT_TRUE(type_compatible(t.type, t.type)) << t.name;
}

// Int widens to Double (the only implicit conversion), never the other way
TEST(TypeTest, IntWidensToDoubleOnly)
{
    EXPECT_TRUE(type_compatible(TYPE_DOUBLE, TYPE_INT));
    EXPECT_FALSE(type_compatible(TYPE_INT, TYPE_DOUBLE));
}

// Any as the target (print, toStr parameters) accepts every type
TEST(TypeTest, AnyTargetAcceptsEverything)
{
    for (const NamedType& t : basic_types())
        EXPECT_TRUE(type_compatible(TYPE_ANY, t.type)) << t.name;
}

// Any as the source is not assignable to a concrete type
TEST(TypeTest, AnySourceNotAssignableToConcreteType)
{
    for (const NamedType& t : basic_types()) {
        if (t.type != TYPE_ANY) {
            EXPECT_FALSE(type_compatible(t.type, TYPE_ANY)) << t.name;
        }
    }
}

// Full matrix: compatible only when equal, Int -> Double, or target is Any
TEST(TypeTest, CompatibilityMatrix)
{
    for (const NamedType& target : basic_types()) {
        for (const NamedType& source : basic_types()) {
            const bool expected = target.type == source.type
                || target.type == TYPE_ANY
                || (target.type == TYPE_DOUBLE && source.type == TYPE_INT);
            EXPECT_EQ(type_compatible(target.type, source.type), expected)
                << "target " << target.name << ", source " << source.name;
        }
    }
}

TEST(TypeTest, CompatibleWithNullIsFalse)
{
    EXPECT_FALSE(type_compatible(nullptr, TYPE_INT));
    EXPECT_FALSE(type_compatible(TYPE_INT, nullptr));
    EXPECT_FALSE(type_compatible(TYPE_ANY, nullptr));
    EXPECT_FALSE(type_compatible(nullptr, nullptr));
}
