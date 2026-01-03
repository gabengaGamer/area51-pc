//==============================================================================
//
//  ConvertUtils.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ConvertUtils.h"

#include <QFile>
#include <QtEndian>

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

xbool IsPowerOfTwo(s32 Value)
{
    return (Value > 0) && ((Value & (Value - 1)) == 0);
}

//==============================================================================

xbitmap::format ConvertFormatString(const QString& Format)
{
    if (Format == "32_RGBA_8888") return xbitmap::FMT_32_RGBA_8888;
    if (Format == "32_RGBU_8888") return xbitmap::FMT_32_RGBU_8888;
    if (Format == "32_ARGB_8888") return xbitmap::FMT_32_ARGB_8888;
    if (Format == "32_URGB_8888") return xbitmap::FMT_32_URGB_8888;
    if (Format == "24_RGB_888") return xbitmap::FMT_24_RGB_888;
    if (Format == "16_RGBA_4444") return xbitmap::FMT_16_RGBA_4444;
    if (Format == "16_ARGB_4444") return xbitmap::FMT_16_ARGB_4444;
    if (Format == "16_RGBA_5551") return xbitmap::FMT_16_RGBA_5551;
    if (Format == "16_RGBU_5551") return xbitmap::FMT_16_RGBU_5551;
    if (Format == "16_ARGB_1555") return xbitmap::FMT_16_ARGB_1555;
    if (Format == "16_URGB_1555") return xbitmap::FMT_16_URGB_1555;
    if (Format == "16_RGB_565") return xbitmap::FMT_16_RGB_565;
    if (Format == "32_BGRA_8888") return xbitmap::FMT_32_BGRA_8888;
    if (Format == "32_BGRU_8888") return xbitmap::FMT_32_BGRU_8888;
    if (Format == "32_ABGR_8888") return xbitmap::FMT_32_ABGR_8888;
    if (Format == "32_UBGR_8888") return xbitmap::FMT_32_UBGR_8888;
    if (Format == "24_BGR_888") return xbitmap::FMT_24_BGR_888;
    if (Format == "16_BGRA_4444") return xbitmap::FMT_16_BGRA_4444;
    if (Format == "16_ABGR_4444") return xbitmap::FMT_16_ABGR_4444;
    if (Format == "16_BGRA_5551") return xbitmap::FMT_16_BGRA_5551;
    if (Format == "16_BGRU_5551") return xbitmap::FMT_16_BGRU_5551;
    if (Format == "16_ABGR_1555") return xbitmap::FMT_16_ABGR_1555;
    if (Format == "16_UBGR_1555") return xbitmap::FMT_16_UBGR_1555;
    if (Format == "16_BGR_565") return xbitmap::FMT_16_BGR_565;
    if (Format == "P8_RGBA_8888") return xbitmap::FMT_P8_RGBA_8888;
    if (Format == "P8_RGBU_8888") return xbitmap::FMT_P8_RGBU_8888;
    if (Format == "P8_ARGB_8888") return xbitmap::FMT_P8_ARGB_8888;
    if (Format == "P8_URGB_8888") return xbitmap::FMT_P8_URGB_8888;
    if (Format == "P8_RGB_888") return xbitmap::FMT_P8_RGB_888;
    if (Format == "P8_RGBA_4444") return xbitmap::FMT_P8_RGBA_4444;
    if (Format == "P8_ARGB_4444") return xbitmap::FMT_P8_ARGB_4444;
    if (Format == "P8_RGBA_5551") return xbitmap::FMT_P8_RGBA_5551;
    if (Format == "P8_RGBU_5551") return xbitmap::FMT_P8_RGBU_5551;
    if (Format == "P8_ARGB_1555") return xbitmap::FMT_P8_ARGB_1555;
    if (Format == "P8_URGB_1555") return xbitmap::FMT_P8_URGB_1555;
    if (Format == "P8_RGB_565") return xbitmap::FMT_P8_RGB_565;
    if (Format == "P4_RGBA_8888") return xbitmap::FMT_P4_RGBA_8888;
    if (Format == "P4_RGBU_8888") return xbitmap::FMT_P4_RGBU_8888;
    if (Format == "P4_ARGB_8888") return xbitmap::FMT_P4_ARGB_8888;
    if (Format == "P4_URGB_8888") return xbitmap::FMT_P4_URGB_8888;
    if (Format == "P4_RGB_888") return xbitmap::FMT_P4_RGB_888;
    if (Format == "P4_RGBA_4444") return xbitmap::FMT_P4_RGBA_4444;
    if (Format == "P4_ARGB_4444") return xbitmap::FMT_P4_ARGB_4444;
    if (Format == "P4_RGBA_5551") return xbitmap::FMT_P4_RGBA_5551;
    if (Format == "P4_RGBU_5551") return xbitmap::FMT_P4_RGBU_5551;
    if (Format == "P4_ARGB_1555") return xbitmap::FMT_P4_ARGB_1555;
    if (Format == "P4_URGB_1555") return xbitmap::FMT_P4_URGB_1555;
    if (Format == "P4_RGB_565") return xbitmap::FMT_P4_RGB_565;
    if (Format == "DXT1") return xbitmap::FMT_DXT1;
    if (Format == "DXT2") return xbitmap::FMT_DXT2;
    if (Format == "DXT3") return xbitmap::FMT_DXT3;
    if (Format == "DXT4") return xbitmap::FMT_DXT4;
    if (Format == "DXT5") return xbitmap::FMT_DXT5;
    if (Format == "A8") return xbitmap::FMT_A8;
    return xbitmap::FMT_NULL;
}

//==============================================================================

xbitmap::format RemapFormatForHobbit(xbitmap::format Format)
{
    if (Format == xbitmap::FMT_DXT3)
        return xbitmap::FMT_A8;
    if (Format == xbitmap::FMT_A8)
        return xbitmap::FMT_DXT3;
    return Format;
}

//==============================================================================

void PatchHobbitFormatInFile(const QString& FileName, xbitmap::format Format)
{
    QFile File(FileName);
    if (!File.open(QIODevice::ReadWrite))
    {
        x_DebugMsg("xbmpViewer: failed to open xbmp for patch: %s\n", FileName.toLocal8Bit().constData());
        return;
    }

    const qint64 HeaderSize = 32;
    QByteArray Header = File.read(HeaderSize);
    if (Header.size() != HeaderSize)
    {
        x_DebugMsg("xbmpViewer: failed to read xbmp header: %s\n", FileName.toLocal8Bit().constData());
        return;
    }

    const s32 FormatValue = static_cast<s32>(Format);
    const u32 FormatLE = qToLittleEndian(static_cast<u32>(FormatValue));
    memcpy(Header.data() + 28, &FormatLE, sizeof(u32));

    if (!File.seek(0) || File.write(Header) != HeaderSize)
    {
        x_DebugMsg("xbmpViewer: failed to write xbmp header: %s\n", FileName.toLocal8Bit().constData());
        return;
    }
}
