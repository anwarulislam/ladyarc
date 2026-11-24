/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QUrl>

namespace Ladybird {

struct ArcPin {
    QString id;
    QString title;
    QUrl url;
    QList<ArcPin> children;
};

struct ArcSpace {
    QString id;
    QString title;
    QList<ArcPin> pins;
};

inline QList<ArcSpace> parse_arc_sidebar(QByteArray const& bytes)
{
    QList<ArcSpace> spaces;
    auto containers = QJsonDocument::fromJson(bytes).object().value("sidebar").toObject().value("containers").toArray();
    QSet<QString> space_ids;
    for (auto value : containers) {
        auto container = value.toObject();
        QHash<QString, QJsonObject> items;
        auto read_records = [](QJsonArray const& records, auto visit) {
            QString key;
            for (auto record : records) {
                if (record.isString()) {
                    key = record.toString();
                    continue;
                }
                auto obj = record.toObject();
                auto id = obj.value("id").toString(key);
                if (!id.isEmpty() && !obj.isEmpty())
                    visit(id, obj);
                key.clear();
            }
        };
        read_records(container.value("items").toArray(), [&](QString const& id, QJsonObject const& item) { items.insert(id, item); });
        QSet<QString> visited;
        auto append = [&](auto&& self, QString const& id, QList<ArcPin>& into, int depth) -> void {
            if (depth > 64 || visited.contains(id) || !items.contains(id))
                return;
            visited.insert(id);
            auto item = items.value(id);
            auto data = item.value("data").toObject();
            ArcPin node { id, item.value("title").toString(), {}, {} };
            if (data.contains("tab")) {
                auto tab = data.value("tab").toObject();
                node.url = QUrl(tab.value("savedURL").toString(), QUrl::StrictMode);
                if (!node.url.isValid() || node.url.host().isEmpty() || (node.url.scheme() != "https" && node.url.scheme() != "http"))
                    return;
                if (node.title.isEmpty())
                    node.title = tab.value("savedTitle").toString(node.url.toDisplayString());
                into.append(std::move(node));
                return;
            }
            auto folder = data.contains("list") || (!node.title.isEmpty() && !data.contains("itemContainer") && !data.contains("splitView"));
            for (auto child : item.value("childrenIds").toArray())
                self(self, child.toString(), folder ? node.children : into, depth + 1);
            if (folder) {
                if (node.title.isEmpty())
                    node.title = QStringLiteral("Imported Folder");
                into.append(std::move(node));
            }
        };
        read_records(container.value("spaces").toArray(), [&](QString const& id, QJsonObject const& obj) {
            if (space_ids.contains(id))
                return;
            auto refs = obj.value("newContainerIDs").toArray();
            if (refs.isEmpty())
                refs = obj.value("containerIDs").toArray();
            if (refs.isEmpty())
                return;
            space_ids.insert(id);
            ArcSpace space { id, obj.value("title").toString().trimmed().left(72), {} };
            if (space.title.isEmpty())
                space.title = QString("Space %1").arg(spaces.size() + 1);
            for (qsizetype i = 0; i + 1 < refs.size(); i += 2) {
                if (refs.at(i).toObject().contains("pinned") || refs.at(i).toString() == "pinned") {
                    append(append, refs.at(i + 1).toString(), space.pins, 0);
                    break;
                }
            }
            spaces.append(std::move(space));
        });
    }
    return spaces;
}

}
