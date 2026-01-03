//==============================================================================
//
//  FileListModel.h
//
//==============================================================================

#ifndef XBMP_FILE_LIST_MODEL_H
#define XBMP_FILE_LIST_MODEL_H

//==============================================================================
//  INCLUDES
//==============================================================================

#include "XBMPViewerConfig.h"

#include <QAbstractTableModel>
#include <QString>
#include <QVector>

//==============================================================================
//  STRUCTS
//==============================================================================

struct FileRecord
{
    QString         Name;
    QString         Path;
    s64             SizeBytes;
    s32             Width;
    s32             Height;
    s32             BitDepth;
    s32             Mips;
    xbitmap::format FormatId;
    QString         Format;
    QString         Type;
};

//==============================================================================
//  FileListModel CLASS
//==============================================================================

class FileListModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit FileListModel              (QObject* pParent = NULL);

    s32 rowCount                        (const QModelIndex& Parent = QModelIndex()) const override;
    s32 columnCount                     (const QModelIndex& Parent = QModelIndex()) const override;
    QVariant data                       (const QModelIndex& Index, s32 Role = Qt::DisplayRole) const override;
    QVariant headerData                 (s32 Section, Qt::Orientation Orientation, s32 Role = Qt::DisplayRole) const override;

    void                SetGameType     (const QString& GameType);
    void                SetDirectory    (const QString& Path);
    const FileRecord*   GetFile         (s32 Row) const;
    s64                 GetTotalBytes   (void) const;
    s32                 GetFileCount    (void) const;

private:
    QString FormatFileSizeKB            (s64 Bytes) const;

private:
    QVector<FileRecord> m_Files;
    QString             m_GameType;
};

//==============================================================================
#endif // XBMP_FILE_LIST_MODEL_H
//==============================================================================