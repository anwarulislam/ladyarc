# Ladyarc feature checklist

Ladyarc uses Ladybird's Qt frontend and browser engine. This checklist follows the [Bye Arc feature catalog](https://byearc.com/pick/). Check a feature when the listed interaction works through the browser UI.

## Release and priority features

- [ ] Build an installable browser from a published release. The Ladyarc workflow also builds development snapshots on changes to the browser. Check both platform jobs and their packages before marking this complete.
- [x] Peek at a web link from its context menu. The preview opens over the source window. Done or Escape dismisses it. Open as Tab moves the live preview into the source window.
- [x] Open an external web link in a Little Arc window. File → New Little Arc Window starts a quick search (Command+Option+N on macOS, Ctrl+Alt+N on Linux). Open in Browser keeps the live tab.
- [x] Choose Spaces → New Space in the browser menu. Switch with the sidebar picker, a sideways trackpad swipe over its tabs, or Command+Option+[ / ] (Ctrl+Alt+[ / ] on Linux). Each Space keeps its open and pinned tabs. Session restore and Reopen Closed Tab retain Space membership.

## Organization

- [x] Vertical sidebar. The tab layout menu offers left, right and all-in-sidebar layouts. Show Sidebar toggles its expanded state.
- [ ] Assign profiles to Spaces and share a profile between Spaces.
- [ ] Show global favorites across Spaces.
- [ ] Pin tabs and organize them in nested Space folders. Window-level pinning already exists.
- [ ] Subscribe a live folder to an external feed.
- [ ] Archive inactive tabs automatically and recover them. The session store already supports reopening manually closed tabs.
- [ ] Set a color or gradient per Space.

## Browsing

- [ ] Display multiple tabs in split view.
- [ ] Search tabs, history, bookmarks and commands from the new-tab command bar. The omnibox and tab search supply parts of this interaction.
- [ ] Route external links to a Space or profile by rule.
- [ ] Save per-site styles, scripts or element customizations.
- [ ] Start picture-in-picture automatically on tab switch. The browser already has a picture-in-picture window.
- [ ] Control background playback from a sidebar mini player.
- [ ] Switch tabs in most-recently-used order while holding Control.
- [x] Copy Clean Link removes common campaign and click identifiers with Command+Shift+C or Ctrl+Shift+C. DevTools remains available on F12 and Command/Ctrl+Shift+I.
- [ ] Capture a region, an element or a full page. Region and full-page capture already exist.
- [x] Drag a completed file from the toolbar downloads popover onto a file drop target or into another application.

## Compatibility and extras

Desktop Arc interactions have implementation priority. Cross-device services, mobile, DRM, browser extensions and platform credentials remain tracked here.

- [ ] Import Arc Spaces, pinned tabs and folders.
- [ ] Run Chrome Web Store extensions, including a password manager, blocker and page modifier.
- [ ] Play DRM-protected streams with a licensed content decryption module.
- [ ] Use platform passkeys with WebAuthn.
- [ ] Sync Spaces, pins and settings between devices.
- [ ] Access desktop Spaces from a mobile app.
- [x] Built-in content blocking. The site settings popover exposes the blocker and its per-site toggle.
- [ ] Remap browser shortcuts in Settings.
- [ ] Run full uBlock Origin with Manifest V2 request blocking.
