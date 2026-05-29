#pragma once

#include <QString>

/// Full-text search result with BM25 ranking and a highlighted snippet.
struct SearchResult {
    qint64  noteId   = -1;
    QString title;
    QString snippet;   // plain-text with <mark>...</mark> highlights
    double  bm25      = 0.0;

    bool operator<(const SearchResult &other) const {
        return bm25 > other.bm25;  // descending by relevance
    }
};
