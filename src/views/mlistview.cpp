/***************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (directui@nokia.com)
**
** This file is part of libmeegotouch.
**
** If you have questions regarding the use of this file, please contact
** Nokia at directui@nokia.com.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file LICENSE.LGPL included in the packaging
** of this file.
**
****************************************************************************/

#include <QList>
#include <MDebug>
#include <QVector>
#include <QTimer>
#include <QStyleOptionGraphicsItem>
#include <MList>
#include <MWidgetView>
#include <MSeparator>
#include <MPannableViewport>

#include "mlistview.h"
#include "mlistview_p.h"

//// MListView /////
MListView::MListView(MWidgetController *widgetController) : MWidgetView(widgetController), d_ptr(NULL)
{
    controller = dynamic_cast<MList *>(widgetController);
}

MListView::~MListView()
{
    delete d_ptr;
}

void MListView::init()
{
    Q_ASSERT(controller);

    delete d_ptr;

    if (model()->columns() > 1) {
        if (model()->showGroups())
            d_ptr = new MMultiColumnListViewPrivate;
        else
            d_ptr = new MPlainMultiColumnListViewPrivate;
    } else {
        if (model()->showGroups())
            d_ptr = new MGroupHeaderListViewPrivate;
        else
            d_ptr = new MPlainListViewPrivate;
    }

    if (model()->showGroups())
        d_ptr->setHeadersCreator(new MDefaultHeadersCreator(style()->groupHeaderObjectName()));

    d_ptr->q_ptr = this;
    d_ptr->controller = dynamic_cast<MList *>(controller);
    d_ptr->createSeparators();
    d_ptr->updateSeparators();

    connectSelectionModel();

    d_ptr->resetModel(model());
}

void MListView::updateData(const QList<const char *>& modifications)
{
    MWidgetView::updateData(modifications);

    const char *member;
    for (int i = 0; i < modifications.count(); i++) {
        member = modifications[i];

        if (member == MListModel::ItemModel || member == MListModel::ShowGroups || member == MListModel::Columns || member == MListModel::CellCreator) {
            if(model()->itemModel())
            {
                init();
                updateGeometry();
            }
        } else if (member == MListModel::SelectionModel) {
            connectSelectionModel();
        } else if (member == MListModel::ScrollToIndex) {
            scrollTo(model()->scrollToIndex(), static_cast<MList::ScrollHint>(model()->scrollHint()));
        }
    }
}

void MListView::connectSelectionModel()
{
    disconnect(this, SLOT(selectionChanged(QItemSelection, QItemSelection)));
    if (model()->selectionModel()) {
        connect(model()->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
                this, SLOT(selectionChanged(QItemSelection, QItemSelection)));
    }
}

void MListView::setupModel()
{
    MWidgetView::setupModel();

    init();
    updateGeometry();
}

void MListView::applyStyle()
{
    MWidgetView::applyStyle();

    if (d_ptr) {
        d_ptr->clearVisibleItemsArray();
        d_ptr->updateSeparators();
        d_ptr->updateSeparatorSize();
        d_ptr->setHeadersCreator(new MDefaultHeadersCreator(style()->groupHeaderObjectName()));
    }
}

QSizeF MListView::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    if (!d_ptr)
        return MWidgetView::sizeHint(which, constraint);

    return QSizeF(-1, d_ptr->totalHeight());
}

void MListView::setGeometry(const QRectF &rect)
{
    if (d_ptr) {
        d_ptr->viewWidth = rect.width();
        d_ptr->updateItemSize();
        d_ptr->updateSeparatorSize();
        relayoutItemsInExposedRect();
    }

    MWidgetView::setGeometry(rect);
}

void MListView::relayoutItemsInExposedRect()
{
    if (d_ptr->model && model()->cellCreator()) {
        int rowCount = d_ptr->model->rowCount();

        if(rowCount)
        {
            QModelIndex firstVisibleRow = d_ptr->locateVisibleIndexAt(d_ptr->viewportTopLeft.y());
            d_ptr->updateFirstVisibleRow(firstVisibleRow);
            QModelIndex lastVisibleRow = d_ptr->locateVisibleIndexAt(
                d_ptr->viewportTopLeft.y() + d_ptr->viewportVisibleHeight - 1); // - 1 to select from inside viewport
            d_ptr->updateLastVisibleRow(lastVisibleRow);

            QPoint firstVisibleItemPos(0, d_ptr->locatePosOfItem(firstVisibleRow));
            QPoint lastVisibleItemPos(0, d_ptr->locatePosOfItem(lastVisibleRow));
            d_ptr->removeInvisibleItems(firstVisibleItemPos, lastVisibleItemPos);

            d_ptr->createVisibleItems(firstVisibleRow, lastVisibleRow);
        } else {
            d_ptr->clearVisibleItemsArray();
        }
    }
}

void MListView::drawForeground(QPainter *painter, const QStyleOptionGraphicsItem *option) const
{
    Q_UNUSED(painter);

    QRectF exposedRect(option->exposedRect);
    if ((d_ptr->viewportTopLeft != exposedRect.topLeft()) || (d_ptr->viewportVisibleHeight < exposedRect.height()) || (d_ptr->forceRepaint)) {
        d_ptr->forceRepaint = false;

        d_ptr->viewportTopLeft = exposedRect.topLeft();
        if (d_ptr->viewportVisibleHeight < exposedRect.height())
            d_ptr->viewportVisibleHeight = exposedRect.height();

        d_ptr->exposedRectChanged(exposedRect);
        const_cast<MListView *>(this)->relayoutItemsInExposedRect();
    }
}

void MListView::drawBackground(QPainter *painter, const QStyleOptionGraphicsItem *option) const
{
    d_ptr->drawSeparators(painter, option);
}

void MListView::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    if (!model()->firstVisibleItem().isValid() && !model()->lastVisibleItem().isValid())
        return;

    const MCellCreator *cellCreator = model()->cellCreator();

    int firstVisibleRow = d_ptr->indexToFlatRow(model()->firstVisibleItem());
    int lastVisibleRow = d_ptr->indexToFlatRow(model()->lastVisibleItem());
    int topLeftRow = d_ptr->indexToFlatRow(topLeft);
    int bottomRightRow = d_ptr->indexToFlatRow(bottomRight);

    int top = qMax(topLeftRow, firstVisibleRow);
    int bottom = qMin(bottomRightRow, lastVisibleRow);

    for (int i = top; i <= bottom; i++) {
        QModelIndex cellIndex = d_ptr->flatRowToIndex(i);
        if (!d_ptr->isGroupHeader(cellIndex)) {
            MWidget *cell = d_ptr->findCellAtRow(i);
            cellCreator->updateCell(cellIndex, cell);
        }
    }
}

/*!
 * This slot is called when items are inserted under the given \a parent.
 * The changed items are those from \a start to \a end inclusive.
 *
 * \sa QAbstractItemView::rowsInserted(), exposedRectUpdated()
 */
void MListView::rowsInserted(const QModelIndex &parent, int start, int end)
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);

    layoutChanged();
}

/*!
 * This slot is called when items under the given \a parent are removed.
 * The removed items are those from \a start to \a end inclusive.
 *
 * \sa rowsInserted(), exposedRectUpdated()
 */
void MListView::rowsRemoved(const QModelIndex &parent, int start, int end)
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);

    layoutChanged();
}

void MListView::layoutChanged()
{
    d_ptr->layoutChanged();

    updateGeometry();
    d_ptr->clearVisibleItemsArray();
}

void MListView::modelReset()
{
    layoutChanged();
}

void MListView::rowsMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd,
                                const QModelIndex &destinationParent, int destinationRow)
{
    Q_UNUSED(sourceParent);
    Q_UNUSED(sourceStart);
    Q_UNUSED(sourceEnd);
    Q_UNUSED(destinationParent);
    Q_UNUSED(destinationRow);

    layoutChanged();
}

void MListView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    d_ptr->selectionChange(selected, deselected);
}

void MListView::itemClick()
{
    QObject *s = sender();
    MWidget *senderWidget = qobject_cast<MWidget *>(s);
    if (senderWidget)
        d_ptr->cellClicked(senderWidget);
}

void MListView::scrollTo(const QModelIndex &index, MList::ScrollHint hint)
{
    if (index.isValid()) {
        int cellPosition = d_ptr->locatePosOfItem(index);

        MPannableViewport *pannableViewport =
            MListViewPrivateNamespace::findParentWidgetOfType<MPannableViewport>(controller);

        if (pannableViewport) {
            QPointF targetPosition = pannableViewport->position();
            QPointF listPosition(controller->mapToItem(pannableViewport->widget(), 0, 0));

            qreal pannableViewportHeight = pannableViewport->boundingRect().height();
            switch (hint) {
            case MList::PositionAtTopHint:
                targetPosition.setY(controller->pos().y() + cellPosition);
                break;

            case MList::PositionAtBottomHint:
                targetPosition.setY(cellPosition + d_ptr->itemHeight + listPosition.y() - pannableViewportHeight);
                break;

            case MList::PositionAtCenterHint:
                targetPosition.setY(listPosition.y() + cellPosition + d_ptr->itemHeight / 2 - pannableViewportHeight / 2);
                break;

            case MList::EnsureVisibleHint:
                if (cellPosition < d_ptr->viewportTopLeft.y()) {
                    targetPosition.setY(controller->pos().y() + cellPosition);
                } else if (cellPosition + d_ptr->itemHeight > d_ptr->viewportTopLeft.y() + pannableViewportHeight) {
                    targetPosition.setY(cellPosition + d_ptr->itemHeight + listPosition.y() - pannableViewportHeight);
                }

                break;
            }

            targetPosition.setY(qMax(targetPosition.y(), (qreal)0));
            targetPosition.setY(qMin(targetPosition.y(), pannableViewport->widget()->boundingRect().height() - pannableViewportHeight));

            pannableViewport->setPosition(targetPosition);
        }
    }
}

M_REGISTER_VIEW_NEW(MListView, MList)
