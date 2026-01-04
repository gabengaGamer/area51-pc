//==============================================================================
//
//  FileListView.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "FileListView.h"

#include <QHeaderView>

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

FileListView::FileListView(QWidget* pParent)
    : QTableView(pParent)
{
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSortingEnabled(TRUE);
    setShowGrid(FALSE);
    setWordWrap(FALSE);
    setAlternatingRowColors(TRUE);
    setEditTriggers(QAbstractItemView::NoEditTriggers);

    QHeaderView* pHeader = horizontalHeader();
    pHeader->setStretchLastSection(TRUE);
    pHeader->setSectionsMovable(TRUE);
    pHeader->setSectionsClickable(TRUE);
    pHeader->setSortIndicatorShown(TRUE);
    pHeader->setHighlightSections(FALSE);

    verticalHeader()->setDefaultSectionSize(18);
    setContextMenuPolicy(Qt::CustomContextMenu);
}
