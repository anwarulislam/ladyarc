/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <UI/Qt/Application.h>
#include <UI/Qt/ArcSidebar.h>
#include <UI/Qt/BookmarkStore.h>
#include <UI/Qt/BrowserWindow.h>
#include <UI/Qt/StringUtils.h>

#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QUuid>

namespace Ladybird {

static String imported_id(QString const& key)
{
    static QUuid const import_namespace("{b3bc92a0-637c-4a34-b34e-12a09cc94f77}");
    return ak_string_from_qstring(QUuid::createUuidV5(import_namespace, key.toUtf8()).toString(QUuid::WithoutBraces));
}

static Vector<BookmarkItem> convert_pins(QString const& space, QList<ArcPin> const& pins, UnixDateTime now)
{
    Vector<BookmarkItem> children;
    for (auto const& pin : pins) {
        auto id = imported_id("pin/" + space + "/" + pin.id);
        auto title = ak_string_from_qstring(pin.title);
        if (pin.url.isEmpty())
            children.append({ move(id), now, now, BookmarkItem::Folder { move(title), convert_pins(space, pin.children, now) } });
        else
            children.append({ move(id), now, now, BookmarkItem::Bookmark { ak_url_from_qurl(pin.url), move(title), {} } });
    }
    return children;
}

void BrowserWindow::import_arc_spaces()
{
    auto path = QFileDialog::getOpenFileName(this, "Import Arc Spaces", {}, "Arc sidebar (StorableSidebar.json *.json)");
    if (path.isEmpty())
        return;
    QFile file(path);
    constexpr qint64 max_size = 64 * 1024 * 1024;
    if (!file.open(QIODevice::ReadOnly) || file.size() > max_size) {
        QMessageBox::warning(this, "Import Arc Spaces", "Choose a readable Arc sidebar file smaller than 64 MiB.");
        return;
    }
    auto bytes = file.read(max_size + 1);
    auto spaces = bytes.size() <= max_size ? parse_arc_sidebar(bytes) : QList<ArcSpace> {};
    if (spaces.isEmpty()) {
        QMessageBox::warning(this, "Import Arc Spaces", "No Arc Spaces were found in this file.");
        return;
    }
    auto& store = Application::bookmark_store();
    auto& settings = Application::desktop_preferences();
    int imported = 0;
    auto now = UnixDateTime::now();
    for (auto const& space : spaces) {
        auto id = imported_id("space/" + space.id);
        if (store.find_item_by_id(id).has_value())
            continue;
        auto name = space.title;
        for (int suffix = 2; m_spaces.contains(name); ++suffix)
            name = QString("%1 (%2)").arg(space.title.left(62)).arg(suffix);
        settings.setValue("pinned-folders/" + QString::fromLatin1(QUrl::toPercentEncoding(name)), qstring_from_ak_string(id));
        m_spaces.append(name);
        store.add_imported_folder({ AK::move(id), now, now, BookmarkItem::Folder { ak_string_from_qstring(name + " · Pinned"), convert_pins(space.id, space.pins, now) } });
        ++imported;
    }
    if (imported && !m_space_sidebar)
        create_space_sidebar();
    refresh_space_sidebar();
    QMessageBox::information(this, "Import Arc Spaces", QString("Imported %1 of %2 Spaces.").arg(imported).arg(spaces.size()));
}

}
