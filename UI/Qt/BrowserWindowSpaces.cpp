/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <UI/Qt/Application.h>
#include <UI/Qt/BrowserWindow.h>
#include <UI/Qt/Menu.h>
#include <UI/Qt/StringUtils.h>

#include <QComboBox>
#include <QDockWidget>
#include <QInputDialog>
#include <QListWidget>
#include <QMenuBar>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace Ladybird {

void BrowserWindow::initialize_spaces()
{
    if (m_is_popup_window == IsPopupWindow::Yes)
        return;
    auto* menu = m_ladybird_menu->addMenu("Spaces");
    menuBar()->addMenu(menu);
    auto* create = menu->addAction("New Space…");
    connect(create, &QAction::triggered, this, [this] {
        auto name = QInputDialog::getText(this, "New Space", "Space name").trimmed().left(72);
        if (!name.isEmpty())
            switch_space(name);
    });
    auto* previous = menu->addAction("Previous Space");
    auto* next = menu->addAction("Next Space");
    previous->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_BracketLeft));
    next->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_BracketRight));
    addAction(previous);
    addAction(next);
    connect(previous, &QAction::triggered, this, [this] { cycle_space(-1); });
    connect(next, &QAction::triggered, this, [this] { cycle_space(1); });
    auto* move_menu = menu->addMenu("Move Current Tab to Space");
    connect(move_menu, &QMenu::aboutToShow, this, [this, move_menu] {
        move_menu->clear();
        for (auto const& name : m_spaces) {
            auto* action = move_menu->addAction(name);
            action->setEnabled(current_tab() && name != m_active_space);
            connect(action, &QAction::triggered, this, [this, name] {
                if (auto* tab = current_tab())
                    assign_space(*tab, name);
            });
        }
    });
}

void BrowserWindow::assign_space(Tab& tab, QString const& name)
{
    if (name.isEmpty() || tab.space_name() == name)
        return;
    if (auto id = tab.session_tab_id(); id.has_value()) {
        if (auto result = session().set_tab_space(*id, ak_string_from_qstring(name)); result.is_error()) {
            dbgln("Unable to store tab Space: {}", result.error());
            return;
        }
    }
    tab.set_space_name(name);
    if (!m_spaces.contains(name))
        m_spaces.append(name);
    if (&tab == current_tab()) {
        m_active_space = name;
        m_last_space_tab.insert(name, &tab);
    }
    if (m_spaces.size() > 1 && !m_space_sidebar)
        create_space_sidebar();
    refresh_space_sidebar();
}

void BrowserWindow::switch_space(QString const& name)
{
    if (!m_spaces.contains(name))
        m_spaces.append(name);
    if (!m_space_sidebar && m_spaces.size() > 1)
        create_space_sidebar();

    m_active_space = name;
    auto* last = m_last_space_tab.value(name).data();
    if (last && last->space_name() == name && tab_index(last) >= 0)
        activate_tab(tab_index(last));
    else {
        for (int i = 0; i < tab_count(); ++i) {
            if (tab_at(i)->space_name() == name) {
                activate_tab(i);
                refresh_space_sidebar();
                return;
            }
        }
        open_new_tab();
    }
    refresh_space_sidebar();
}

void BrowserWindow::cycle_space(int delta)
{
    auto count = m_spaces.size();
    if (count > 1) {
        auto next = (m_spaces.indexOf(m_active_space) + delta + count) % count;
        auto name = m_spaces.at(next);
        switch_space(name);
    }
}

void BrowserWindow::show_tab_in_space(int position)
{
    int last = -1;
    for (int i = 0; i < tab_count(); ++i) {
        if (tab_at(i)->space_name() != m_active_space)
            continue;
        last = i;
        if (position-- == 0)
            break;
    }
    if (last >= 0)
        activate_tab(last);
}

void BrowserWindow::cycle_visible_tab(int delta)
{
    auto count = tab_count();
    auto index = m_tabs_container->current_index();
    for (int i = 0; i < count; ++i) {
        index = (index + delta + count) % count;
        if (tab_at(index)->space_name() == m_active_space) {
            activate_tab(index);
            return;
        }
    }
}

void BrowserWindow::create_space_sidebar()
{
    m_space_sidebar = new QDockWidget("Spaces", this);
    m_space_sidebar->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_space_sidebar->setFeatures(QDockWidget::NoDockWidgetFeatures);
    auto* panel = new QWidget(m_space_sidebar);
    auto* layout = new QVBoxLayout(panel);
    m_space_picker = new QComboBox(panel);
    m_space_picker->setAccessibleName("Current Space");
    layout->addWidget(m_space_picker);
    m_space_tab_list = new QListWidget(panel);
    m_space_tab_list->setAccessibleName("Tabs in this Space");
    m_space_tab_list->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_space_tab_list);
    auto* add = new QPushButton("New Tab", panel);
    layout->addWidget(add);
    connect(add, &QPushButton::clicked, this, &BrowserWindow::open_new_tab);
    connect(m_space_picker, &QComboBox::textActivated, this, [this](QString const& name) { switch_space(name); });
    connect(m_space_tab_list, &QListWidget::currentRowChanged, this, [this](int row) {
        auto tab = m_listed_space_tabs.value(row);
        if (tab && tab_index(tab) >= 0)
            activate_tab(tab_index(tab));
    });
    connect(m_space_tab_list, &QWidget::customContextMenuRequested, this, [this](QPoint point) {
        auto* item = m_space_tab_list->itemAt(point);
        auto tab = m_listed_space_tabs.value(m_space_tab_list->row(item));
        if (tab && tab_index(tab) >= 0)
            execute_context_menu(*tab->context_menu(), m_space_tab_list->viewport()->mapToGlobal(point));
    });
    m_space_sidebar->setWidget(panel);
    addDockWidget(Qt::LeftDockWidgetArea, m_space_sidebar);
    m_tabs_container->set_space_sidebar_visible(true);
    update_tabs_display();
    refresh_space_sidebar();
}

void BrowserWindow::refresh_space_sidebar()
{
    if (!m_space_sidebar || m_space_refresh_pending)
        return;
    m_space_refresh_pending = true;
    QTimer::singleShot(0, this, [this] {
        m_space_refresh_pending = false;
        QSignalBlocker picker_blocker(m_space_picker);
        QSignalBlocker list_blocker(m_space_tab_list);
        if (m_space_picker->count() != m_spaces.size()) {
            m_space_picker->clear();
            m_space_picker->addItems(m_spaces);
        }
        m_space_picker->setCurrentText(m_active_space);
        m_space_tab_list->clear();
        m_listed_space_tabs.clear();
        for (int i = 0; i < tab_count(); ++i) {
            auto* tab = tab_at(i);
            if (tab->space_name() != m_active_space)
                continue;
            auto title = tab->title().isEmpty() ? QStringLiteral("New Tab") : tab->title();
            auto* item = new QListWidgetItem(tab->tab_icon(), title, m_space_tab_list);
            item->setToolTip(title);
            auto font = item->font();
            font.setBold(tab->is_pinned());
            item->setFont(font);
            if (tab->is_pinned())
                item->setData(Qt::AccessibleDescriptionRole, "Pinned tab");
            m_listed_space_tabs.append(tab);
            if (tab == current_tab())
                m_space_tab_list->setCurrentItem(item);
        }
    });
}

bool BrowserWindow::handle_space_swipe(QWheelEvent& event)
{
    if (event.phase() == Qt::ScrollBegin || event.phase() == Qt::ScrollEnd) {
        m_space_swipe_distance = 0;
        m_space_swipe_consumed = false;
        return false;
    }
    auto delta = event.pixelDelta();
    if (delta.isNull() || qAbs(delta.x()) <= qAbs(delta.y()) || event.phase() == Qt::NoScrollPhase)
        return false;
    if (!m_space_swipe_consumed && event.phase() != Qt::ScrollMomentum) {
        m_space_swipe_distance += delta.x();
        if (qAbs(m_space_swipe_distance) >= 70) {
            m_space_swipe_consumed = true;
            cycle_space(m_space_swipe_distance < 0 ? 1 : -1);
        }
    }
    event.accept();
    return true;
}

}
