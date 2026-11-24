/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <QStringList>
#include <QUrl>

namespace Ladybird {

// Work on the serialized fields so a surviving parameter keeps its encoding, order and duplicates.
inline QString clean_link(QString const& url)
{
    if (!url.startsWith("https://") && !url.startsWith("http://"))
        return url;
    auto query = url.indexOf('?');
    auto fragment = url.indexOf('#');
    if (query < 0 || (fragment >= 0 && query > fragment))
        return url;
    auto fields = url.mid(query + 1, fragment < 0 ? -1 : fragment - query - 1).split('&');
    QStringList kept;
    for (auto const& field : fields) {
        auto name = QUrl::fromPercentEncoding(field.section('=', 0, 0).toUtf8()).toLower();
        if (name == "signature" || name == "sig" || name == "x-amz-signature" || name == "x-goog-signature")
            return url;
        if (name.startsWith("utm_") || name == "fbclid" || name == "gclid" || name == "dclid"
            || name == "msclkid" || name == "mc_cid" || name == "mc_eid")
            continue;
        kept.append(field);
    }
    if (kept.size() == fields.size())
        return url;
    auto result = url.left(query);
    if (!kept.isEmpty())
        result += '?' + kept.join('&');
    if (fragment >= 0)
        result += url.mid(fragment);
    return result;
}

}
