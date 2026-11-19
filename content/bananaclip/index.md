+++
title = "Banana Clip"
description = "Support and privacy policy for the Banana Clip iPhone app"
+++

Banana Clip is an iPhone app that cuts short clips out of whatever is playing
on your own Plex, Jellyfin or Emby server. Questions, bugs and feature
requests: [ynadji@gmail.com](mailto:ynadji@gmail.com).

## Privacy policy

*Last updated November 19, 2026.*

Banana Clip has no server of its own, no accounts, no analytics and no
third-party SDKs. Nothing you do in the app is sent to me.

**What stays on your phone.** The address of your media server, the sign-in
token it gives you (kept in the iOS keychain), your clip settings and the clips
you make. Clips are saved in the app's own folder, which you can see in the
Files app, and, only if you turn that option on, in your Photos library. All of
this is removed when you delete the app, except for clips you chose to save to
Photos.

**What talks to your media server.** The app reads the list of active sessions,
item details and the media file itself from your server, using your
credentials, over your own network or whatever remote access you have set up
for the server. The server sees the app the same way it sees any other client.

**Plex.** Signing in with Plex uses Plex's standard device link flow: the app
asks plex.tv for a link code, you approve it on plex.tv, and plex.tv returns a
token and the list of your servers. That exchange is subject to
[Plex's privacy policy](https://www.plex.tv/about/privacy-legal/). The app does
not contact plex.tv after that except to refresh the server list.

**Jellyfin and Emby.** Signing in sends your user name and password directly to
the server address you typed, and nowhere else.

**Purchases.** The optional upgrade is sold through Apple's In-App Purchase.
Apple handles payment; the app only learns whether the purchase is active.

**Children.** The app is not directed at children under 13 and collects no
personal information from anyone.

**Changes.** If this policy changes, the new version will be posted at this
address with a new date.
