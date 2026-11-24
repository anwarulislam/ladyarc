/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <UI/Qt/Application.h>
#include <UI/Qt/Bookmarks.h>
#include <UI/Qt/SuggestionProvider.h>

namespace Ladybird {

SuggestionProvider::SuggestionProvider(WebView::IsPrivate is_private, u64 profile_id)
    : m_profile_id(profile_id)
    , m_autocomplete(*this, is_private)
{
    m_autocomplete.on_autocomplete_query_complete = [this](auto query_id, auto suggestions, auto kind) {
        if (on_suggestions)
            on_suggestions(query_id, move(suggestions), kind);
    };
}

WebView::AutocompleteService& SuggestionProvider::autocomplete_service()
{
    return Application::the().profile_autocomplete(m_profile_id);
}

WebView::Settings& SuggestionProvider::autocomplete_settings()
{
    return Application::settings();
}

void SuggestionProvider::resolve_external_url_handler(URL::URL const& url, WebView::ExternalURLHandlerCallback callback) const
{
    Application::the().resolve_external_url_handler(url, move(callback));
}

Function<void()> SuggestionProvider::fetch_remote_suggestions(WebView::SearchEngine const& engine, String const& query, WebView::IsPrivate is_private, Function<void(RemoteResponse)> callback)
{
    return Application::the().fetch_profile_suggestions(engine, query, is_private, m_profile_id, move(callback));
}

void SuggestionProvider::query(WebView::AutocompleteQueryID query_id, String query, size_t max_suggestions)
{
    auto bookmarks = Application::bookmarks().suggestions_for(query, max_suggestions);
    m_autocomplete.query_autocomplete_engine(query_id, move(query), max_suggestions, move(bookmarks));
}

void SuggestionProvider::cancel()
{
    m_autocomplete.cancel_pending_query();
}

void SuggestionProvider::record_engagement(WebView::OmniboxEngagement engagement)
{
    m_autocomplete.record_engagement(move(engagement));
}

}
