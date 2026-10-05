# Chess with contacts (M9)

> Translated from the Russian original [docs/chess.md](../chess.md); when they differ, the original is current.

A game is played between two MeshCore nodes through direct messages: every command is
an ordinary encrypted `TXT_MSG` text message with a delivery receipt (ACK) and
MeshCore retries. There is no separate packet type, so the messages pass through
any MeshCore repeaters, and the stock MeshCore app shows them as readable
text. The public channel (Public) is not used for moves: its sender is not
authenticated by a key.

Code: `src/Chess.cpp` (rules), `src/ChessNet.cpp` (games, commands, storage),
`src/UiChess.inc` (M9 screen), the “Chess” section of the web page (`web/index.html`).
On the M9 you play on the screen or on the web page, on the Heltec — on the web page of its access
point (the Heltec has no chess screen); the USB commands `chess ...` exist on both boards.

## Commands

A message starts with `♟` (U+265F) and four hexadecimal digits of the game number,
then a space and the command. Text after the known fields is not read — an
explanation for a human goes there.

| Text | Meaning |
|---|---|
| `♟3F2A new w r · шахматы MeshMesh: вы играете чёрными` | invitation; `w`/`b` — the sender's color, `r` — rated (the trailing Russian text reads “MeshMesh chess: you play black”) |
| `♟3F2A yes` / `♟3F2A no` | accept / decline (or cancel your own invitation before the first move) |
| `♟3F2A 5 g1f3 3. Nf3` | move: half-move number, the move in UCI (`e7e8q` — promotion), notation for a human |
| `♟3F2A draw?` / `♟3F2A draw` | offer a draw / accept the offered draw |
| `♟3F2A resign` | resign |
| `♟3F2A sig 1791140000 <base64>` | the signature of a rated game's result (see “ELO rating”) |

Reception: commands are accepted only in direct messages from the contact of that game.
A move counts if the half-move number is the next one, it is the opponent's turn and the move is legal
by the rules. A repeat of the opponent's last move (this happens when an ACK was lost and
the sender retried) is silently skipped; a move out of turn or an illegal move does not
change the game, and a notification appears on the screen. The opponent's reply move and their “yes”
confirm delivery of my move/invitation even if the ACK did not arrive. White's first move
in reply to an invitation where the inviter plays black counts as acceptance.

## Delivery over a poor link

The route is the same as for any MeshCore direct message: while the path is unknown, the message is
flooded through repeaters, the reply brings back the path, and after that it is sent directly along it.
If there is no ACK over the known path, the third attempt is flooded; if that is not
confirmed either, the path is reset, and the next send looks for a route again.

A move is recorded on your own device immediately and is sent as a message; MeshCore makes up to three
attempts (about 1.5 min). If there is no confirmation, the command is marked “not delivered” and
is retried automatically: after 2, 5 and 10 minutes, then every 15 minutes, and also as soon
as the device hears the opponent again (an advert or any of their packets), but not
more often than once every 2 minutes — so as not to occupy the air when the link is one-way. Retries go
one command at a time: first your own last move (it is taken from the game, so a draw
offer or resignation sent later does not replace it), then the last other command.
Confirmation is an ACK or the opponent's reply (their move confirms my move and the acceptance of
the invitation, their agreement confirms my draw offer). After 24 hours without confirmation
automatic retrying stops; R on the M9 or “Resend” (“Повторить отправку”) on the web page send immediately and
start it again. The undelivered-move flag is saved (format `MMC1` version 2,
version 1 is readable); after a restart, retrying starts after one minute.

Both sides determine the end of the game themselves by the same rules: checkmate, stalemate, threefold
repetition, 50 moves without a capture or a pawn move, insufficient material, the limit of
512 half-moves (draw). There is no separate result message.

A person with the stock MeshCore app can play by typing the commands manually
(for example `♟3F2A 2 e7e5`) — this was tested with a board that shows these messages
as an ordinary chat; there is no convenient interface for them.

## ELO rating

A game is rated (the default) or friendly: the choice is the “Game” row on the M9 challenge screen
(the F key or a tap), a switch on the web page, `chess invite NODE w|b|r friendly` over USB.
A rated challenge carries the `r` mark (`♟3F2A new w r · …`); by accepting it the opponent agrees to a rated game.
Older versions and the companion page do not read the mark and play a friendly game.

**Signing the result.** When a game is over (mate, stalemate, resignation, a draw and so on) and each side
made at least one move, each side, in the main loop (not in the radio handler), builds the same record and
signs it with its MeshCore node key (Ed25519):

| Field | Bytes |
|---|---|
| `MMR1` | 4 |
| White's key, Black's key | 32 + 32 |
| game number, result (1 White, 2 Black, 3 draw), reason | 2 + 1 + 1 |
| half-move count, the first 16 bytes of SHA-256 of the move list | 2 + 16 |
| flags (bit 0 — rated) | 1 |

These 91 bytes and the signer's clock (Unix time, 0 when the clock is not set) are signed. The signature
goes out as `♟3F2A sig 1791140000 <88 base64 characters>` with an ACK and the same retries as a move;
R resends it at once. The opponent's signature is checked with their key from the contact (remembered at the
challenge); if the sides' records differ (a lost move, for example), the signature does not match and the game
stays unrated. A record with both signatures checked is kept; a second copy of the same record does not count.

**Calculation.** ELO is replayed from the kept records in one order (by the earlier of the two times,
then by the moves digest), so the same records give the same numbers on every device. The starting
rating is 1500; K = 32 for a player's first 20 counted games, then 20; the expected score is
`1/(1+10^((Rb−Ra)/400))`. Three games with one opponent per UTC day count; the rest are kept and shown
but do not change the rating (“over the daily limit”). The score against each opponent
(+wins =draws −losses) covers all kept games.

**Storage.** `/meshmesh/rating.bin` in LittleFS (`MMR1`, players, records, CRC-32; written through
`rating.new`/`rating.old` like the games). Up to 128 records on ESP32 and 40 on GAT562; when full, the
oldest record moves into the players' base and the ratings do not change. Up to 32 players.

**Where it shows.** M9 and T-Deck: the title “Chess · ELO 1520”, the “Rating” tab (◂▸): the rating, the
change over the latest games and a chart, players with their rating and your score; on the challenge
screen — the opponent's ELO and what a win gives and a loss takes; on the board of a finished game —
“ELO 1516 (+16)” or the signature state (“their signature”, “signatures”). Web page and app: the
“Games / Rating” tabs, the “Rated / Friendly” switch, the forecast next to contacts, the rating line on the board
(`GET /api/chess?rating=1`, USB `chess rating`; the `rated`, `sign`, `elo_before`, `elo_after` fields in the
game list). The Heltec and GAT562 show the rating change in a pop-up.

Signing does not rule out boosting a rating with a second device of your own: it proves that both keys
accepted the result, not that different people hold them. The daily limit restricts it.

## Tournament (Swiss system)

The organiser is any board with MeshMesh: M9, T-Deck, Heltec, GAT562 and the community boards (on the GAT562
the tournament fitted once the images were split by language).
A tournament is created on the M9 (“Chess” → the “Tournaments” tab → “Create a tournament”: name,
players from the chat contacts, the number of rounds and the time a move), on the web page or over USB
`tour create ROUNDS HOURS ID,ID,... NAME`. Up to 10 players with the organiser, up to 9 rounds; up to
three tournaments are kept at once. Every tournament game is rated.

**How it goes.** The organiser sends invitations; the tournament starts by itself when everyone has
answered and at least two agreed (S on the tournament page starts at once with those who agreed). The
organiser's device pairs: round 1 by lot (seeded by the tournament number), later the players are
ordered by score, then by rating, and inside a score group the top half plays the bottom half; nobody
meets twice while it can be avoided (a search with backtracking); White goes to the player with fewer
Whites, then to the one who had Black last round. With an odd number of players the lowest player
without a bye gets a point without a game. Each player gets their pairing: colour, game number, the
opponent's key and name (the opponent becomes a contact even if their advert is not heard; messages go
by flood until a path is found). The game starts at once, without a challenge; when every board is busy it
takes the place of the oldest finished game whose rating and report are done (the Heltec has no chess
screen, so nobody opens such games), and if there is none it is created on a later attempt, every 30 s. When a game is over, each
player's device reports the result to the organiser by itself; two matching reports give the result,
differing ones a “dispute” that the organiser settles (W/B/D on the M9 pairings screen, buttons on the web
page, USB `tour result TOUR GAME w|b|d`). When every game of the round is decided, the organiser sends the
standings (points and the Buchholz score — the sum of the opponents' points) and the next round's pairings;
after the last one — the result. Starting and closing a round run in the main loop, not in the radio handler:
the pairing search and sending did not fit its stack (an M9 panic on the hardware showed it).

| Text (`♞` — U+265E, tournament number) | Meaning |
|---|---|
| `♞7C01 inv 5 24 7 Кубок двора` | invitation: rounds, hours a move, players, name |
| `♞7C01 yes` / `♞7C01 no` | a player's answer |
| `♞7C01 pl 7A8544:Name,C99339:Name` | the players (first 6 ID digits and a name up to 12 bytes), in parts |
| `♞7C01 r2 w 3F2A <base64 key> Name` / `♞7C01 r2 bye` | the round's pairing: colour, game number, the opponent's key and name / a bye |
| `♞7C01 res 3F2A w` | a player's result: `w`, `b`, `d` |
| `♞7C01 st 2 7A8544:3:5,…` | the standings after a round: ID, points×2, Buchholz×2 |
| `♞7C01 end 5 7A8544:7:21,…` / `♞7C01 cancel` | finished (with the final standings, so the place is right whichever message arrives first) / cancelled |

Tournament commands go as direct messages with ACK; unconfirmed ones are resent after 2, 5, 10 and every
15 minutes for a day (a queue of up to 40 messages is saved with the tournaments in `/meshmesh/tour.bin`;
after a restart resending starts in a minute). The time a move is a guide for now: there is no loss on
time. If the organiser disappears, the tournament stops — organising is not handed over.

**Where it shows.** M9 and T-Deck: the “Tournaments” tab (the list, an invitation, gathering players, the
tournament page with your game of the round, the standings and the games — ◂▸, OK — to the game), the
tournament strip on the board, pop-ups and a lit screen for a round's pairing and the result. Heltec and GAT562 — a
pop-up with the news; answering an invitation and the standings are on the web page or in the app, and the
GAT562 can play on its screen too. Web page and app: the “Tournaments” tab
with the same actions and a creation form (`GET /api/tour`, USB `tour`).

## Ledger exchange

Records with both signatures spread between nodes, so everyone who has them shows the same ratings — also
for games a node did not see. A record carries both players' keys and signatures, so a node checks it by itself
and accepts it from any contact; a forged or changed record is dropped.

| Text (`♜` — U+265C) | Meaning |
|---|---|
| `♜sum 12 A1B2C3D4` | the number of newest records (up to 64) and a digest of their numbers |
| `♜ok` | the ledgers match |
| `♜inv 1/2 1A2B3C4D,5E6F7A8B,…` | the newest record numbers (a number is the first 4 bytes of the record's SHA-256), 15 a message |
| `♜want 1A2B3C4D,…` | missing records (at most 10 a session) |
| `♜rec 1A2B3C4D 2/3 <base64>` | a record in three parts: without `MMR1` and the flags, both times, both signatures (222 bytes) |

A session: one node sends `sum`; if they differ the other sends its list, the first asks for what it lacks and
sends its own list, the second asks for what it lacks. Messages go at most every 6 s and only when the radio is
free; incoming ones are handled in the main loop. A sync starts by itself when a node is heard that played rated
games or has synced before — at most every 6 hours per node; by hand — S on the M9 “Rating” tab, the “Sync the
ledger now” button on the web page, USB `chess sync` (every such node heard in a day) or `chess sync NODE_ID`.
When the ledger is full, a record older than the oldest kept one is not accepted: it may already be in the
players' base and would count twice. Nodes with ledgers of different size (128 records on ESP32, 40 on GAT562)
may differ slightly in ratings after old records move into the base. `chess rating clear` over USB empties the
ledger (for example before restoring it by syncing with the neighbours).

## M9 screen

Main menu → “Chess”: the list of games (first those where your move or reply is needed),
“New game” → choice of chat contact and color (white, black, random). The board
is turned towards the player; the last move and check are highlighted, a picked-up piece shows its
legal moves. Keys: arrows — cursor, OK — pick up/put down, BACK — return the
piece or go back to the list, D — offer/accept a draw, X twice — resign,
R — resend an undelivered command immediately (otherwise the retry is automatic), N — rematch with colors swapped, DEL — decline,
cancel an invitation or delete a finished game, H — help. Moves are written in
Russian notation (Кр, Ф, Л, С, К) if the Russian interface is selected.

Up to 6 games are stored at a time; a new one takes a free slot or the oldest
viewed finished game. An invitation for which there is no room is declined
automatically.

## Web page

The “Chess” tile: the list of games, color choice and inviting a chat contact, a touch board
(touching your piece shows its moves, touching a square makes the move; on promotion —
choice of piece), offering and accepting a draw, resigning and deleting with a double press,
rematch, resending an undelivered command, the move record. The rules and the games stay
in the firmware: `GET /api/chess` — the list and the latest news, `GET /api/chess?id=3F2A` —
the move record and legal moves, actions — `POST /api/command` (`chess move 3F2A e2e4`).
The page polls the device every 3 s.

## With a stock MeshCore companion (website page)

https://ikrasnodymov.github.io/meshmesh/chess/ — the same chess page, but without MeshMesh on your own
device: it connects to a board running the stock MeshCore Companion firmware over USB (Web Serial)
or Bluetooth (Web Bluetooth) and plays through it with ordinary direct messages. The opponent plays
on MeshMesh (M9, Heltec, GAT562) or on the same page.

- The rules and games run in the browser: `web/chess-companion.js` is a port of `src/Chess.cpp` and
  `src/ChessNet.cpp` (the same commands, reception, retries, automatic resending of undelivered commands).
  `tools/chess_site.py` builds the page from `web/index.html` and this file
  (`tools/pages.py` puts it into `chess/` on the website); the file is not part of the firmware.
- The companion protocol (`upstream/meshcore-stock/examples/companion_radio/MyMesh.cpp`):
  `DEVICE_QUERY` (version 3), `APP_START`, setting the clock (forward only, as in the app),
  `GET_CONTACTS`, `SEND_TXT_MSG`, ACK — `PUSH_SEND_CONFIRMED`, reception — `MSG_WAITING` and
  `SYNC_NEXT_MESSAGE`. USB: `<`/`>` frames with a length; Bluetooth: Nordic UART, one frame per
  notification, PIN — handled by the OS. Sending works as in the MeshCore app: up to three attempts with
  one timestamp, and the path is reset before the third (flooding).
- Games are stored in the browser (`localStorage`), separately for each companion's key. A move
  made without a connection is marked “not delivered” and is sent after connecting.
- The opponent's moves that arrive while the page is closed wait in the companion's queue (16 messages in
  the USB build) and are fetched on connection. Automatic resending of your own moves works while the page is open.
- Only one app can connect to a companion: while the page is open, do not connect the MeshCore app to that
  device. Direct and channel messages unrelated to chess are taken from the queue by the page
  and shown in the “Other messages” section; they will no longer be in the MeshCore app.
- Chrome or Edge on a computer and Chrome on Android; Safari, iOS and Firefox support neither Web Serial
  nor Web Bluetooth. The page opens a previously allowed USB port by itself on load.
- Verified on October 2, 2026 (`docs/verification.md`): USB companion v1.17.1 on the GAT562 against the M9 over the radio
  (`chess_companion_check.py`) and in Chrome over Web Serial. Bluetooth and Chrome on Android were not checked.

## Notifications

- M9 and T-Deck: a pop-up message “Heltec V4: Nf3, your move” (on any page except
  the lock screen), a sound, a “waiting for move” badge on the tile. News that needs a move
  or an answer (the opponent's move, a challenge, a draw offer) and the end of a game light a dark screen.
- Lock screen: a game card above the messages card — the opponent, their move and the state
  (“Challenges you: you play Black”), with “more: N” on the right when other games wait for a move.
  It shows the game with the latest unseen news, otherwise the newest game waiting for a move.
  Holding OK unlocks and opens that game at once. The “Lock screen” setting (`lock_details`,
  “Screen & device”, web page): “Hidden” leaves “Chess · N games wait” and “Unlock to read”
  without names, moves and senders.
- Heltec: a “Chess” window on the OLED with the same text, three LED flashes, the screen
  wakes up; a click closes the window.
- Web page: a pop-up message about the opponent's move, an invitation, a draw offer,
  the result and an undelivered command; the number of games waiting for a move in the
  tab title and on the tile; optionally a sound (a button on the “Chess” page) and vibration
  where the browser supports it. System browser notifications (Notification API)
  are not available at `http://192.168.4.1`: browsers allow them only over HTTPS.
  The page reports a move while it is open. On the site's `chess/` page (HTTPS) the
  “Background notifications” button turns on system notifications while the tab is in the
  background; a click opens the game.
- Android app in the background: one notification per game (the opponent's move, a challenge,
  an accepted challenge, a draw offer, the result), replaced by the next news of that game; a tap
  opens the board. The text is built from the game list (`/api/chess`), not from the board's message,
  so it is Russian whatever the screen language. Games are checked as soon as the board receives
  something, and at least every 32 s.

## Storage

`/meshmesh/chess.bin` in the internal LittleFS: the `MMC1` header, version, games
(opponent, number, color, state, the last sent command and its status,
the list of moves) and a CRC-32. It is written to `chess.new`, the previous copy is renamed
to `chess.old`, then the new one to `chess.bin`; if the main file is corrupted when read,
`chess.old` is used. On load the moves are replayed by the rules; a game with an
illegal move is discarded. A command that had no ACK before the restart
is considered undelivered and is retried automatically. The SD card and chat history are not touched.
Version 3 added the rated mark, the opponent's key and both signatures to a game; version 1 and 2 files
are read, while older firmware does not read a version 3 file (its games are lost after a downgrade). The rating
is a separate file, `rating.bin` (see “ELO rating”).

## Checks

- `tools/ui_preview/build.sh m9 DIR` — besides the screens, a whole tournament on the organiser's side: the
  wizard with six contacts, the automatic start after every answer (not in the radio handler), four rounds with
  both players' reports, a dispute settled by the organiser; the checks: one game per pair and one bye per round,
  no repeated games, at most one bye per player, alternating colours, the total of points and the standings
  order; the player's side — an invitation, the player list, a pairing with the opponent's key starts the game,
  the standings.
- `tools/chess/rating_check.sh` — on a computer: signatures with the MeshCore Ed25519 code (another clock or
  result breaks the signature), base64, ELO 1500 → 1516/1484, the expected change, the daily limit,
  a repeated record, reading the file back and moving old records into the base.
- `tools/chess/rules_check.sh` — on a computer: perft for six reference positions
  (including castling, en passant and promotions), FEN, the move record,
  checkmate, stalemate, repetition, insufficient material, rejection of an illegal move list.
- `tools/ui_preview/build.sh m9 DIR` — the chess screens (list, selection, board, a piece with
  its moves, promotion, an undelivered move, a draw offer, help).
- `tools/web_usb_bridge.py --port PORT` — the web page in a computer browser with the board's
  answers over USB (actions really go out over the radio); for visual checking
  without connecting to the device's Wi-Fi. Real HTTP over Wi-Fi, including
  `/api/chess`, is checked by `wifi_probe.py`.
- `tools/chess_check.py` — over the radio between the M9 and the Heltec: an invitation from the Heltec, acceptance on
  the M9 screen, a game to checkmate with matching positions after every half-move, a repeat and
  an illegal move as text are ignored, commands do not get into the chat history,
  no restarts. Part of `finish_on_hardware.py` after the radio check.
- `node tools/chess/companion_check.cjs` — on a computer, for the companion page: perft, the move
  record and the browser rules' game endings, 300 random games checked half-move by half-move against
  `src/Chess.cpp` (`tools/chess/replay.cpp`); a game between two instances with a lost ACK, a retry,
  checkmate, automatic resending, a draw and storage; the companion protocol on a simulator (frames with garbage
  in front of them, contacts, ACK, three attempts with a path reset, “not delivered”).
- `tools/chess_companion_check.py --companion PORT` — over the radio: the page code in Node
  (`tools/chess/companion_node.cjs`) through `usb_tcp_bridge.py --raw` and the stock USB companion
  against the M9 on MeshMesh: an invitation from the page and checkmate, a repeat and an illegal move as text are ignored,
  an invitation from the M9 and a draw by agreement, an ordinary message lands in “Other messages” as a single copy,
  an ACK for every command, matching positions, the M9 does not restart and does not show the commands in the chat.
