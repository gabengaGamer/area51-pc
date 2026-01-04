//==============================================================================
//
//  ExplorerPanel.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ExplorerPanel.h"

#include <QHeaderView>
#include <QCompleter>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QFileSystemModel>
#include <QLineEdit>
#include <QTreeView>
#include <QVBoxLayout>

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

ExplorerPanel::ExplorerPanel(QWidget* pParent)
    : QWidget(pParent)
    , m_pModel(NULL)
    , m_pTree(NULL)
    , m_pPathEdit(NULL)
    , m_pPathCompleter(NULL)
{
    m_pModel = new QFileSystemModel(this);
    m_pModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Drives);
    m_pModel->setIconProvider(new QFileIconProvider());
    m_pModel->setRootPath(QString());

    m_pTree = new QTreeView(this);
    m_pTree->setModel(m_pModel);
    m_pTree->setRootIndex(m_pModel->index(QString()));

    m_pTree->setSortingEnabled(TRUE);
    m_pTree->sortByColumn(0, Qt::AscendingOrder);

    QHeaderView* pHeader = m_pTree->header();
    pHeader->setSectionsClickable(TRUE); 
    pHeader->setSortIndicatorShown(TRUE);
    pHeader->setStretchLastSection(TRUE);
    pHeader->setHighlightSections(FALSE);	
    pHeader->setSectionResizeMode(QHeaderView::ResizeToContents);

    m_pPathEdit = new QLineEdit(this);
    m_pPathCompleter = new QCompleter(m_pModel, this);
    m_pPathCompleter->setCompletionMode(QCompleter::PopupCompletion);
    m_pPathCompleter->setFilterMode(Qt::MatchStartsWith);
    m_pPathCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    m_pPathEdit->setCompleter(m_pPathCompleter);

    QVBoxLayout* pLayout = new QVBoxLayout(this);
    pLayout->setContentsMargins(4, 4, 4, 4);
    pLayout->addWidget(m_pPathEdit);
    pLayout->addWidget(m_pTree, 1);

    connect(m_pTree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &ExplorerPanel::OnTreeSelectionChanged);
    connect(m_pPathEdit, &QLineEdit::editingFinished,
            this, &ExplorerPanel::OnPathEntered);
    connect(m_pPathCompleter, QOverload<const QString&>::of(&QCompleter::activated),
            this, [this](const QString& Path)
            {
                SetPath(Path);
            });
    connect(m_pModel, &QFileSystemModel::directoryLoaded,
            this, [this](const QString&)
            {
                m_pTree->viewport()->update();
            });

    SetPath(QDir::currentPath());
}

//==============================================================================

void ExplorerPanel::ConfigureDock(QDockWidget* pDock) const
{
    if (!pDock)
        return;

    pDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
}

//==============================================================================

void ExplorerPanel::SetPath(const QString& Path)
{
    QFileInfo Info(Path);
    if (!Info.exists())
        return;

    const QString DirPath = Info.isDir() ? Info.absoluteFilePath() : Info.absolutePath();
    m_CurrentPath = DirPath;
    m_pPathEdit->setText(DirPath);
    UpdateSelection(DirPath);

    emit PathSelected(DirPath);
}

//==============================================================================

QString ExplorerPanel::GetPath(void) const
{
    return m_CurrentPath;
}

//==============================================================================

void ExplorerPanel::OnTreeSelectionChanged(const QModelIndex& Current, const QModelIndex& Previous)
{
    (void)Previous;
    if (!Current.isValid())
        return;

    const QString Path = m_pModel->filePath(Current);
    if (Path.isEmpty())
        return;

    m_CurrentPath = Path;
    m_pPathEdit->setText(Path);
    emit PathSelected(Path);
}

//==============================================================================

void ExplorerPanel::OnPathEntered(void)
{
    const QString Path = m_pPathEdit->text();
    if (Path.isEmpty())
        return;

    SetPath(Path);
}

//==============================================================================

void ExplorerPanel::UpdateSelection(const QString& Path)
{
    const QModelIndex Index = m_pModel->index(Path);
    if (!Index.isValid())
        return;

    m_pTree->setCurrentIndex(Index);
    m_pTree->scrollTo(Index);
}
