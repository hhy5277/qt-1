// Commit: 282441f72a7704aadc5525a360430d0c3d49aea6
/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qsgimage_p.h"
#include "qsgimage_p_p.h"

#include "qsgcontext.h"
#include "adaptationlayer.h"

#include <QtGui/qpainter.h>

QT_BEGIN_NAMESPACE

QSGImage::QSGImage(QSGItem *parent)
    : QSGImageBase(*(new QSGImagePrivate), parent)
{
}

QSGImage::QSGImage(QSGImagePrivate &dd, QSGItem *parent)
    : QSGImageBase(dd, parent)
{
}

QSGImage::~QSGImage()
{
}

QPixmap QSGImage::pixmap() const
{
    Q_D(const QSGImage);
    return d->pix.pixmap();
}

void QSGImage::setPixmap(const QPixmap &pix)
{
    Q_D(QSGImage);
    if (!d->url.isEmpty())
        return;
    d->setPixmap(pix);
}

void QSGImagePrivate::setPixmap(const QPixmap &pixmap)
{
    Q_Q(QSGImage);
    pix.setPixmap(pixmap);

    q->setImplicitWidth(pix.width());
    q->setImplicitHeight(pix.height());
    status = pix.isNull() ? QSGImageBase::Null : QSGImageBase::Ready;

    q->update();
    q->pixmapChange();
}

QSGImage::FillMode QSGImage::fillMode() const
{
    Q_D(const QSGImage);
    return d->fillMode;
}

void QSGImage::setFillMode(FillMode mode)
{
    Q_D(QSGImage);
    if (d->fillMode == mode)
        return;
    d->fillMode = mode;
    update();
    updatePaintedGeometry();
    emit fillModeChanged();
}

qreal QSGImage::paintedWidth() const
{
    Q_D(const QSGImage);
    return d->paintedWidth;
}

qreal QSGImage::paintedHeight() const
{
    Q_D(const QSGImage);
    return d->paintedHeight;
}

void QSGImage::updatePaintedGeometry()
{
    Q_D(QSGImage);

    if (d->fillMode == PreserveAspectFit) {
        if (!d->pix.width() || !d->pix.height())
            return;
        qreal widthScale = width() / qreal(d->pix.width());
        qreal heightScale = height() / qreal(d->pix.height());
        if (widthScale <= heightScale) {
            d->paintedWidth = width();
            d->paintedHeight = widthScale * qreal(d->pix.height());
        } else if(heightScale < widthScale) {
            d->paintedWidth = heightScale * qreal(d->pix.width());
            d->paintedHeight = height();
        }
        if (widthValid() && !heightValid()) {
            setImplicitHeight(d->paintedHeight);
        }
        if (heightValid() && !widthValid()) {
            setImplicitWidth(d->paintedWidth);
        }
    } else if (d->fillMode == PreserveAspectCrop) {
        if (!d->pix.width() || !d->pix.height())
            return;
        qreal widthScale = width() / qreal(d->pix.width());
        qreal heightScale = height() / qreal(d->pix.height());
        if (widthScale < heightScale) {
            widthScale = heightScale;
        } else if(heightScale < widthScale) {
            heightScale = widthScale;
        }

        d->paintedHeight = heightScale * qreal(d->pix.height());
        d->paintedWidth = widthScale * qreal(d->pix.width());
    } else {
        d->paintedWidth = width();
        d->paintedHeight = height();
    }
    emit paintedGeometryChanged();
}

void QSGImage::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QSGImageBase::geometryChanged(newGeometry, oldGeometry);
    updatePaintedGeometry();
}

QRectF QSGImage::boundingRect() const
{
    Q_D(const QSGImage);
    return QRectF(0, 0, qMax(width(), d->paintedWidth), qMax(height(), d->paintedHeight));
}

Node *QSGImage::updatePaintNode(Node *oldNode, UpdatePaintNodeData *data)
{
    Q_D(QSGImage);

    if (d->pix.pixmap().isNull() || width() <= 0 || height() <= 0) {
        delete oldNode;
        return 0;
    }

    TextureNodeInterface *node = static_cast<TextureNodeInterface *>(oldNode);
    if (!node) { 
        d->pixmapChanged = true;
        node = QSGContext::current->createTextureNode();
    }

    if (d->pixmapChanged) {
        QSGTextureManager *tm = QSGContext::current->textureManager();
        QSGTextureRef ref = tm->upload(d->pix.pixmap().toImage());
        node->setTexture(ref);
        d->pixmapChanged = false;
    }

    QRectF targetRect;
    QRectF sourceRect;
    bool clampToEdge = true;

    switch (d->fillMode) {
    default:
    case Stretch:
        targetRect = QRectF(0, 0, width(), height());
        sourceRect = d->pix.rect();
        break;

    case PreserveAspectFit:
        targetRect = QRectF((width() - d->paintedWidth) / 2., (height() - d->paintedHeight) / 2.,
                            d->paintedWidth, d->paintedHeight);
        sourceRect = d->pix.rect();
        break;

    case PreserveAspectCrop: {
        targetRect = QRect(0, 0, width(), height());
        qreal wscale = width() / qreal(d->pix.width());
        qreal hscale = height() / qreal(d->pix.height());

        if (wscale > hscale) {
            int src = (hscale / wscale) * qreal(d->pix.height());
            sourceRect = QRectF(0, (d->pix.height() - src) / 2, d->pix.width(), src);
        } else {
            int src = (wscale / hscale) * qreal(d->pix.width());
            sourceRect = QRectF((d->pix.width() - src) / 2, 0, src, d->pix.height());
        }
    }
        break;

    case Tile:
        targetRect = QRectF(0, 0, width(), height());
        sourceRect = QRectF(0, 0, width(), height());
        clampToEdge = false;
        break;

    case TileHorizontally:
        targetRect = QRectF(0, 0, width(), height());
        sourceRect = QRectF(0, 0, width(), d->pix.height());
        clampToEdge = false;
        break;

    case TileVertically:
        targetRect = QRectF(0, 0, width(), height());
        sourceRect = QRectF(0, 0, d->pix.width(), height());
        clampToEdge = false;
        break;

    };

    QRectF nsrect(sourceRect.x() / d->pix.width(),
                  sourceRect.y() / d->pix.height(),
                  sourceRect.width() / d->pix.width(),
                  sourceRect.height() / d->pix.height());

    node->setRect(targetRect);
    node->setSourceRect(nsrect);
    node->setOpacity(data->opacity);
    node->setClampToEdge(clampToEdge);
    node->setLinearFiltering(d->smooth);
    node->update();

    return node;
}

void QSGImage::pixmapChange()
{
    Q_D(QSGImage);

    updatePaintedGeometry();
    d->pixmapChanged = true;
}

QT_END_NAMESPACE