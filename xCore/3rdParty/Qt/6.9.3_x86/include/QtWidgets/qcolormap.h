// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#ifndef QCOLORMAP_H
#define QCOLORMAP_H

#include <QtWidgets/qtwidgetsglobal.h>
#include <QtGui/qrgb.h>
#include <QtGui/qwindowdefs.h>
#include <QtCore/qatomic.h>
#include <QtCore/qlist.h>

QT_BEGIN_NAMESPACE

class QColor;
class QColormapPrivate;

#if QT_WARN_DEPRECATED_UP_TO >= QT_VERSION_CHECK(6, 11, 0)
# if defined(Q_CC_GNU_ONLY) && Q_CC_GNU < 1300
// GCC 12 and earlier has issues combining __attribute__ and [[]] syntax
#  define QT_DEPRECATED_VERSION_6_11_CUSTOM __attribute__((deprecated))
# else
#  define QT_DEPRECATED_VERSION_6_11_CUSTOM QT_DEPRECATED
# endif
#else
# define QT_DEPRECATED_VERSION_6_11_CUSTOM
#endif

#if QT_REMOVAL_QT7_DEPRECATED_SINCE(6, 11)
class Q_WIDGETS_EXPORT QT_DEPRECATED_VERSION_6_11_CUSTOM QColormap
{
public:
    enum Mode { Direct, Indexed, Gray };

    static void initialize();
    static void cleanup();

    static QColormap instance(int screen = -1);

    QColormap(const QColormap &colormap);
    ~QColormap();

    QColormap &operator=(const QColormap &colormap);

    Mode mode() const;

    int depth() const;
    int size() const;

    uint pixel(const QColor &color) const;
    const QColor colorAt(uint pixel) const;

    const QList<QColor> colormap() const;

private:
    QColormap();
    QColormapPrivate *d;
};
#endif // QT_REMOVAL_QT7_DEPRECATED_SINCE(6, 11)

#undef QT_DEPRECATED_VERSION_6_11_CUSTOM

QT_END_NAMESPACE

#endif // QCOLORMAP_H
