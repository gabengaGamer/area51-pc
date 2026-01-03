//==============================================================================
//
//  FileListModel.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "FileListModel.h"

#include "aux_Bitmap.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

//==============================================================================

namespace
{
    enum
    {
        COLUMN_NAME = 0,
        COLUMN_SIZE,
        COLUMN_DIMENSIONS,
        COLUMN_MIPS,
        COLUMN_TYPE,
        COLUMN_FORMAT,
        COLUMN_COUNT
    };
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

FileListModel::FileListModel(QObject* pParent)
    : QAbstractTableModel(pParent)
    , m_GameType("Area-51")
{
}

//==============================================================================

s32 FileListModel::rowCount(const QModelIndex& Parent) const
{
    if (Parent.isValid())
        return 0;
    return m_Files.size();
}

//==============================================================================

s32 FileListModel::columnCount(const QModelIndex& Parent) const
{
    if (Parent.isValid())
        return 0;
    return COLUMN_COUNT;
}

//==============================================================================

QVariant FileListModel::data(const QModelIndex& Index, s32 Role) const
{
    if (!Index.isValid())
        return QVariant();

    const s32 Row = Index.row();
    if ((Row < 0) || (Row >= m_Files.size()))
        return QVariant();

    const FileRecord& Record = m_Files[Row];

    if (Role == Qt::UserRole)
    {
        switch (Index.column())
        {
        case COLUMN_NAME:
            return Record.Name;
        case COLUMN_SIZE:
            return static_cast<qint64>(Record.SizeBytes);
        case COLUMN_DIMENSIONS:
            return static_cast<qint64>(Record.Width) * static_cast<qint64>(Record.Height);
        case COLUMN_MIPS:
            return Record.Mips;
        case COLUMN_TYPE:
            return Record.Type;
        case COLUMN_FORMAT:
            return Record.Format;
        default:
            break;
        }
    }

    if (Role != Qt::DisplayRole)
        return QVariant();

    switch (Index.column())
    {
    case COLUMN_NAME:
        return Record.Name;
    case COLUMN_SIZE:
        return FormatFileSizeKB(Record.SizeBytes);
    case COLUMN_DIMENSIONS:
        if (Record.Width > 0 && Record.Height > 0 && Record.BitDepth > 0)
            return QString("%1x%2x%3b").arg(Record.Width).arg(Record.Height).arg(Record.BitDepth);
        return QString();
    case COLUMN_MIPS:
        if (Record.Mips > 0)
            return QString::number(Record.Mips);
        return QString();
    case COLUMN_TYPE:
        return Record.Type;
    case COLUMN_FORMAT:
        return Record.Format;
    default:
        break;
    }

    return QVariant();
}

//==============================================================================

QVariant FileListModel::headerData(s32 Section, Qt::Orientation Orientation, s32 Role) const
{
    if (Orientation != Qt::Horizontal || Role != Qt::DisplayRole)
        return QVariant();

    switch (Section)
    {
    case COLUMN_NAME:
        return QString("Name");
    case COLUMN_SIZE:
        return QString("Size");
    case COLUMN_DIMENSIONS:
        return QString("Dimensions");
    case COLUMN_MIPS:
        return QString("Mips");
    case COLUMN_TYPE:
        return QString("Type");
    case COLUMN_FORMAT:
        return QString("Format");
    default:
        break;
    }

    return QVariant();
}

//==============================================================================

void FileListModel::SetDirectory(const QString& Path)
{
    beginResetModel();
    m_Files.clear();

    QDir Dir(Path);
    if (!Dir.exists())
    {
        endResetModel();
        return;
    }

    const QStringList Filters = QStringList()
        << "*.xbmp" << "*.xbm" << "*.bmp" << "*.psd" << "*.tga";

    QDirIterator Iterator(Path, Filters, QDir::Files, QDirIterator::NoIteratorFlags);
    while (Iterator.hasNext())
    {
        const QString FilePath = Iterator.next();
        QFileInfo Info(FilePath);

        FileRecord Record;
        Record.Name = Info.fileName();
        Record.Path = FilePath;
        Record.SizeBytes = Info.size();
        Record.Width = 0;
        Record.Height = 0;
        Record.BitDepth = 0;
        Record.Mips = 0;
        Record.FormatId = xbitmap::FMT_NULL;
        Record.Format = QString();
        Record.Type = Info.suffix().toLower();

        xbitmap::info BitmapInfo;
        const QByteArray PathBytes = FilePath.toLocal8Bit();
        if (auxbmp_Info(PathBytes.constData(), BitmapInfo))
        {
            Record.Width = BitmapInfo.W;
            Record.Height = BitmapInfo.H;
            Record.Mips = BitmapInfo.nMips;
            Record.FormatId = BitmapInfo.Format;

            if (m_GameType == "The Hobbit")
            {
                if (Record.FormatId == xbitmap::FMT_DXT3)
                    Record.FormatId = xbitmap::FMT_A8;
                else if (Record.FormatId == xbitmap::FMT_A8)
                    Record.FormatId = xbitmap::FMT_DXT3;
            }

            const xbitmap::format_info& FormatInfo = xbitmap::GetFormatInfo(Record.FormatId);
            Record.BitDepth = FormatInfo.BPP;
            Record.Format = FormatInfo.pString;
        }

        m_Files.push_back(Record);
    }

    endResetModel();
}

//==============================================================================

const FileRecord* FileListModel::GetFile(s32 Row) const
{
    if ((Row < 0) || (Row >= m_Files.size()))
        return NULL;
    return &m_Files[Row];
}

//==============================================================================

s64 FileListModel::GetTotalBytes(void) const
{
    s64 Total = 0;
    for (s32 i = 0; i < m_Files.size(); ++i)
        Total += m_Files[i].SizeBytes;
    return Total;
}

//==============================================================================

s32 FileListModel::GetFileCount(void) const
{
    return m_Files.size();
}

//==============================================================================

void FileListModel::SetGameType(const QString& GameType)
{
    m_GameType = GameType;
}

//==============================================================================

QString FileListModel::FormatFileSizeKB(s64 Bytes) const
{
    s64 KB = Bytes / 1024;
    QString Raw = QString::number(KB);
    QString WithCommas;
    s32 Count = Raw.length();
    for (s32 i = 0; i < Raw.length(); ++i)
    {
        WithCommas += Raw.at(i);
        Count--;
        if (((Count % 3) == 0) && (Count > 0))
            WithCommas += ',';
    }
    WithCommas += " KB";
    return WithCommas;
}