/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWebView/Autocomplete.h>
#include <LibWebView/Omnibox.h>

namespace Ladybird {

// Makes the suggestions of a location bar from the browsing history, the bookmarks and the search engine.
class SuggestionProvider final : public WebView::OmniboxSuggestionProvider
    , private WebView::AutocompleteHost {
public:
    AK_ALLOC_WITH_KMALLOC;

    explicit SuggestionProvider(WebView::IsPrivate, u64 profile_id = 0);
    virtual ~SuggestionProvider() override = default;

    virtual void query(WebView::AutocompleteQueryID, String, size_t max_suggestions) override;
    virtual void cancel() override;
    virtual void record_engagement(WebView::OmniboxEngagement) override;

private:
    WebView::AutocompleteService& autocomplete_service() override;
    WebView::Settings& autocomplete_settings() override;
    void resolve_external_url_handler(URL::URL const&, WebView::ExternalURLHandlerCallback) const override;
    Function<void()> fetch_remote_suggestions(WebView::SearchEngine const&, String const&, WebView::IsPrivate, Function<void(RemoteResponse)>) override;

    u64 m_profile_id { 0 };
    WebView::Autocomplete m_autocomplete;
};

}
