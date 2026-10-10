#pragma once
#include "core/Document.h"

#include <string>
#include <vector>

// Linear undo history. Each entry keeps the state before and after its edit.
class History {
public:
    struct Entry {
        std::string name;
        DocState before;
        DocState after;
    };

    void push(const std::string& name, DocState before, DocState after);
    bool canUndo() const { return m_index > 0; }
    bool canRedo() const { return m_index < m_entries.size(); }
    const DocState* undo();  // state to restore, or nullptr
    const DocState* redo();
    void jumpTo(size_t applied, Document& doc);  // applied = number of entries in effect
    bool isClean() const { return m_index == m_clean; }
    void markClean() { m_clean = m_index; }
    size_t appliedCount() const { return m_index; }
    const std::vector<Entry>& entries() const { return m_entries; }

private:
    std::vector<Entry> m_entries;
    size_t m_index = 0;
    size_t m_clean = 0;
};
