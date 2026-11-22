#include "block_budget.h"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <string>

using profiler_gui::BlockBudgets;
using profiler_gui::NO_BUDGET;
using profiler_gui::budgetFromMicroseconds;
using profiler_gui::budgetOf;
using profiler_gui::isOverBudget;

TEST(BlockBudget, NoBudgetIsNeverExceeded)
{
    EXPECT_FALSE(isOverBudget(0, NO_BUDGET));
    EXPECT_FALSE(isOverBudget(std::numeric_limits<std::uint64_t>::max(), NO_BUDGET));
}

TEST(BlockBudget, ExceededOnlyStrictlyAboveBudget)
{
    EXPECT_FALSE(isOverBudget(999, 1000));
    EXPECT_FALSE(isOverBudget(1000, 1000));   // exactly on budget = within budget
    EXPECT_TRUE(isOverBudget(1001, 1000));
}

TEST(BlockBudget, MicrosecondsConversion)
{
    EXPECT_EQ(budgetFromMicroseconds(200.0), 200'000U);
    EXPECT_EQ(budgetFromMicroseconds(0.5), 500U);
    EXPECT_EQ(budgetFromMicroseconds(0.0004), 1U);        // positive but < 1 ns -> still a budget
    EXPECT_DOUBLE_EQ(profiler_gui::budgetToMicroseconds(1'500U), 1.5);
}

TEST(BlockBudget, InvalidUserValuesGiveNoBudget)
{
    EXPECT_EQ(budgetFromMicroseconds(0.0), NO_BUDGET);
    EXPECT_EQ(budgetFromMicroseconds(-5.0), NO_BUDGET);
    EXPECT_EQ(budgetFromMicroseconds(std::nan("")), NO_BUDGET);
    EXPECT_EQ(budgetFromMicroseconds(std::numeric_limits<double>::infinity()), NO_BUDGET);
    EXPECT_EQ(budgetFromMicroseconds(profiler_gui::MAX_BUDGET_US * 2), NO_BUDGET);
}

TEST(BlockBudget, SetGetRemove)
{
    BlockBudgets budgets;
    EXPECT_TRUE(budgets.empty());

    budgets.set("Control", 200'000);
    EXPECT_EQ(budgets.get("Control"), 200'000U);
    EXPECT_EQ(budgets.get("Acquire"), NO_BUDGET);

    budgets.set("Control", 150'000);                      // overwrite
    EXPECT_EQ(budgets.get("Control"), 150'000U);

    budgets.set("Control", NO_BUDGET);                    // NO_BUDGET removes
    EXPECT_TRUE(budgets.empty());

    budgets.set("Output", 10);
    budgets.remove("Output");
    EXPECT_EQ(budgets.size(), 0U);
}

TEST(BlockBudget, TableIsIndexedByBlockIdAndSkipsEmptySlots)
{
    BlockBudgets budgets;
    budgets.set("Control", 200'000);
    budgets.set("Output", 50'000);

    // id 2 = an empty descriptor slot (nullptr), id 3 = a run-time named copy of "Control"
    const std::array<const char*, 5> names{"Acquire", "Control", nullptr, "Control", "Output"};
    const auto table = budgets.buildTable(names.size(), [&](std::size_t id) { return names.at(id); });

    ASSERT_EQ(table.size(), names.size());
    EXPECT_EQ(table.at(0), NO_BUDGET);
    EXPECT_EQ(table.at(1), 200'000U);
    EXPECT_EQ(table.at(2), NO_BUDGET);
    EXPECT_EQ(table.at(3), 200'000U);
    EXPECT_EQ(table.at(4), 50'000U);

    EXPECT_EQ(budgetOf(table, 1), 200'000U);
    EXPECT_EQ(budgetOf(table, 99), NO_BUDGET);            // out of range id is safe
}

TEST(BlockBudget, EmptyBudgetsGiveAnAllZeroTable)
{
    const BlockBudgets budgets;
    int calls = 0;
    const auto table = budgets.buildTable(3, [&](std::size_t) { ++calls; return "X"; });
    EXPECT_EQ(table, (std::vector<profiler_gui::budget_ns_t>(3, NO_BUDGET)));
    EXPECT_EQ(calls, 0);                                  // no lookup at all when nothing is configured
}
