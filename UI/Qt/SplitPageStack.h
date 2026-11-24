/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <QPointer>
#include <QScopedValueRollback>
#include <QStackedWidget>

#include <functional>

namespace Ladybird {

class SplitPageStack final : public QStackedWidget {
public:
    explicit SplitPageStack(QWidget* parent)
        : QStackedWidget(parent)
    {
        connect(this, &QStackedWidget::currentChanged, this, [this] {
            if (contains_pane(currentWidget()))
                layout_panes();
            else
                end_split();
        });
        connect(this, &QStackedWidget::widgetRemoved, this, [this] {
            if (indexOf(m_left) < 0 || indexOf(m_right) < 0)
                end_split();
        });
    }

    bool contains_pane(QWidget const* widget) const { return widget && (widget == m_left || widget == m_right); }

    void split_with(QWidget* other)
    {
        auto* current = currentWidget();
        if (!current || !other || other == current || indexOf(other) < 0)
            return;
        end_split();
        m_left = current;
        m_right = other;
        layout_panes();
        if (on_visibility_change)
            on_visibility_change(*other, true);
    }

    void end_split()
    {
        if (!m_left && !m_right)
            return;
        auto left = m_left;
        auto right = m_right;
        m_left.clear();
        m_right.clear();
        setContentsMargins(0, 0, 0, 0);
        for (auto pane : { left, right }) {
            if (pane && pane != currentWidget() && indexOf(pane) >= 0) {
                pane->hide();
                if (on_visibility_change)
                    on_visibility_change(*pane, false);
            }
        }
    }

    std::function<void(QWidget&, bool)> on_visibility_change;

private:
    void resizeEvent(QResizeEvent* event) override
    {
        QStackedWidget::resizeEvent(event);
        layout_panes();
    }

    void layout_panes()
    {
        if (m_laying_out || !m_left || !m_right)
            return;
        QScopedValueRollback guard(m_laying_out, true);
        int left_width = width() / 2;
        bool left_active = currentWidget() == m_left;
        // The stack continues to lay out its current page. Its other visible page uses the reserved half.
        setContentsMargins(left_active ? 0 : left_width + 1, 0, left_active ? width() - left_width : 0, 0);
        auto* other = left_active ? m_right.data() : m_left.data();
        other->setGeometry(left_active ? QRect(left_width + 1, 0, width() - left_width - 1, height()) : QRect(0, 0, left_width, height()));
        other->show();
    }

    QPointer<QWidget> m_left;
    QPointer<QWidget> m_right;
    bool m_laying_out { false };
};

}
