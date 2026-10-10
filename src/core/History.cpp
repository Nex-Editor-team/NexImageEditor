#include "core/History.h"

void History::push(const std::string& name, DocState before, DocState after) {
    m_entries.resize(m_index);
    if (m_clean > m_index) m_clean = size_t(-1);  // clean state was in the discarded redo branch
    m_entries.push_back(Entry{name, std::move(before), std::move(after)});
    m_index = m_entries.size();
}

const DocState* History::undo() {
    if (!canUndo()) return nullptr;
    m_index -= 1;
    return &m_entries[m_index].before;
}

const DocState* History::redo() {
    if (!canRedo()) return nullptr;
    const DocState* s = &m_entries[m_index].after;
    m_index += 1;
    return s;
}

void History::jumpTo(size_t applied, Document& doc) {
    if (applied > m_entries.size()) applied = m_entries.size();
    while (m_index > applied) {
        m_index -= 1;
        doc.restore(m_entries[m_index].before);
    }
    while (m_index < applied) {
        doc.restore(m_entries[m_index].after);
        m_index += 1;
    }
}
