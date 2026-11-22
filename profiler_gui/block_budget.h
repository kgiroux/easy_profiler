/************************************************************************
* file name         : block_budget.h
* ----------------- :
* description       : Time budget per block type (Qt-free, unit-tested).
*                   : A block whose duration exceeds the budget of its type is
*                   : displayed in red in the diagram and in the call-stack tree.
* ----------------- :
* license           : Lightweight profiler library for c++
*                   : Copyright(C) 2016-2019  Sergey Yagovtsev, Victor Zarubkin
*                   :
*                   : Licensed under either of
*                   :     * MIT license (LICENSE.MIT or http://opensource.org/licenses/MIT)
*                   :     * Apache License, Version 2.0, (LICENSE.APACHE or http://www.apache.org/licenses/LICENSE-2.0)
*                   : at your option.
************************************************************************/

#ifndef EASY_PROFILER_GUI_BLOCK_BUDGET_H
#define EASY_PROFILER_GUI_BLOCK_BUDGET_H

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace profiler_gui {

/// Budget in nanoseconds; 0 means "no budget".
using budget_ns_t = std::uint64_t;

constexpr budget_ns_t NO_BUDGET = 0;

/// Largest accepted budget: 1 hour, far beyond any real-time period.
constexpr double MAX_BUDGET_US = 3600.0 * 1e6;

/// True when a block lasting `_durationNs` exceeds a (non-zero) budget. Equality is within budget.
inline bool isOverBudget(std::uint64_t _durationNs, budget_ns_t _budgetNs) noexcept
{
    return _budgetNs != NO_BUDGET && _durationNs > _budgetNs;
}

/// Converts a user value in microseconds to a budget.
/// Returns NO_BUDGET for a non-positive, non-finite or absurdly large value.
/// A positive value below 1 ns is rounded up to 1 ns so that it still is a budget.
inline budget_ns_t budgetFromMicroseconds(double _us) noexcept
{
    if (!std::isfinite(_us) || _us <= 0.0 || _us > MAX_BUDGET_US)
        return NO_BUDGET;

    const auto ns = static_cast<budget_ns_t>(std::llround(_us * 1000.0));
    return ns == 0 ? 1 : ns;
}

inline double budgetToMicroseconds(budget_ns_t _budgetNs) noexcept
{
    return static_cast<double>(_budgetNs) * 1e-3;
}

/** Budgets keyed by block name.

The name is the block descriptor name (the string literal given to EASY_BLOCK / EASY_FUNCTION),
so a budget survives from one capture to the next and applies to every block of that name.
*/
class BlockBudgets
{
    std::unordered_map<std::string, budget_ns_t> m_budgets;

public:

    using map_t = std::unordered_map<std::string, budget_ns_t>;

    /// Sets the budget of a block name; NO_BUDGET removes it.
    void set(const std::string& _name, budget_ns_t _budgetNs)
    {
        if (_budgetNs == NO_BUDGET)
            m_budgets.erase(_name);
        else
            m_budgets[_name] = _budgetNs;
    }

    void remove(const std::string& _name) { m_budgets.erase(_name); }

    void clear() noexcept { m_budgets.clear(); }

    budget_ns_t get(const std::string& _name) const
    {
        const auto it = m_budgets.find(_name);
        return it != m_budgets.end() ? it->second : NO_BUDGET;
    }

    bool empty() const noexcept { return m_budgets.empty(); }

    std::size_t size() const noexcept { return m_budgets.size(); }

    const map_t& all() const noexcept { return m_budgets; }

    /** Builds a lookup table indexed by block id, so that painting never hashes strings.

    \param _count   number of block ids (size of the descriptors list).
    \param _nameOf  callable `const char*(std::size_t id)`; may return nullptr for an empty slot.
    */
    template <class NameOf>
    std::vector<budget_ns_t> buildTable(std::size_t _count, NameOf&& _nameOf) const
    {
        std::vector<budget_ns_t> table(_count, NO_BUDGET);
        if (m_budgets.empty())
            return table;

        for (std::size_t id = 0; id < _count; ++id)
        {
            const char* name = _nameOf(id);
            if (name != nullptr)
                table[id] = get(name);
        }

        return table;
    }
};

/// Budget of a block id in a table built by BlockBudgets::buildTable (NO_BUDGET when out of range).
inline budget_ns_t budgetOf(const std::vector<budget_ns_t>& _table, std::size_t _id) noexcept
{
    return _id < _table.size() ? _table[_id] : NO_BUDGET;
}

} // END of namespace profiler_gui.

#endif // EASY_PROFILER_GUI_BLOCK_BUDGET_H
