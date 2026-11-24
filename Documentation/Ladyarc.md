# Ladyarc feature checklist

Ladyarc uses Ladybird's Qt frontend and browser engine. This checklist follows the [Bye Arc feature catalog](https://byearc.com/pick/). Check a feature when the listed interaction works through the browser UI.

## Release and priority features

- [ ] Build an installable browser from a published release. The Ladyarc workflow also builds a snapshot on manual dispatch and when the workflow itself changes on dev. Check both platform jobs and their packages before marking this complete.
- [x] Peek at a web link from its context menu. The preview opens over the source window. Done or Escape dismisses it. Open as Tab moves the live preview into the source window.
- [x] Open an external web link in a Little Arc window. File → New Little Arc Window starts a quick search (Command+Option+N on macOS, Ctrl+Alt+N on Linux). Open in Browser keeps the live tab.
- [x] Choose Spaces → New Space in the browser menu. Switch with the sidebar picker, a sideways trackpad swipe over its tabs, or Command+Option+[ / ] (Ctrl+Alt+[ / ] on Linux). Each Space keeps its open and pinned tabs. Session restore and Reopen Closed Tab retain Space membership.

## Organization

- [x] Vertical sidebar. The tab layout menu offers left, right and all-in-sidebar layouts. Show Sidebar toggles its expanded state.
- [ ] Assign profiles to Spaces and share a profile between Spaces.
- [x] Spaces → Add Current Page to Favorites pins an icon above the Space picker in every window in the profile. Right-click an icon to remove it. The Favorites bookmark folder also supports editing and reordering.
- [x] Pin Tab saves a bookmark in the current Space. Select a folder before choosing New Pinned Folder to nest it. Right-click a folder to pin the current page there. Activate a saved pin to return to it.
- [ ] Subscribe a live folder to an external feed.
- [x] Spaces → Auto-archive lets you choose 12 hours, one day, one week, or 30 days. Recover an archived page in its original Space from Archived Tabs. Archiving defaults to off and excludes private windows, pins, split panes, active media and pages requesting a before-unload check.
- [x] Choose Space Color sets the sidebar color for a named Space. Use System Color follows the desktop theme again. Private windows keep their colors in memory.

## Browsing

- [x] Choose another open tab from Split View in the browser menu to place it beside the current page. Click a pane to direct the address bar and commands to it. End Split View returns to one page.
- [x] Command/Ctrl+T searches open tabs, bookmark titles and URLs, browsing history, and browser commands. Arrow keys select a result. Enter opens it and Escape dismisses the palette. Private windows omit profile history.
- [x] Spaces → External Link Routing assigns links for the current hostname to a named Space. External web links with no matching rule open in Little Arc. Private windows do not write routing rules.
- [x] Edit Site Boost saves custom CSS for the current web origin. Clear the editor to remove it. The style applies to loaded tabs on that origin and to subsequent visits.
- [x] Enable Automatic Picture in Picture in the browser menu to pop out an eligible playing video in the top-level document when switching away from its tab.
- [x] A sidebar mini player appears for audio in a background tab. Play / Pause targets that tab’s media session and remains available after pausing.
- [x] Hold Control and press Tab to visit tabs in recently used order in the current Space. Shift reverses direction. Releasing Control remembers the selection. The Next and Previous Tab menu items still follow strip order.
- [x] Copy Clean Link removes common campaign and click identifiers with Command+Shift+C or Ctrl+Shift+C. DevTools remains available on F12 and Command/Ctrl+Shift+I.
- [x] Capture Element in the page context menu saves the element under the pointer. The screenshot overlay already supports regions and full pages.
- [x] Drag a completed file from the toolbar downloads popover onto a file drop target or into another application.

## Compatibility and extras

Desktop Arc interactions have implementation priority. Cross-device services, mobile, DRM, browser extensions and platform credentials remain tracked here.

- [x] Spaces → Import Arc Spaces reads a StorableSidebar.json file. It preserves named Spaces, web pins and nested folders. Saved pins open on demand. Importing the same Space again skips it.
- [ ] Run Chrome Web Store extensions, including a password manager, blocker and page modifier.
- [ ] Play DRM-protected streams with a licensed content decryption module.
- [ ] Use platform passkeys with WebAuthn.
- [ ] Sync Spaces, pins and settings between devices.
- [ ] Access desktop Spaces from a mobile app.
- [x] Built-in content blocking. The site settings popover exposes the blocker and its per-site toggle.
- [ ] Remap browser shortcuts in Settings.
- [ ] Run full uBlock Origin with Manifest V2 request blocking.
