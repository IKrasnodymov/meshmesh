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
| `♟3F2A new w · шахматы MeshMesh: вы играете чёрными` | invitation; `w`/`b` — the sender's color (the trailing Russian text reads “MeshMesh chess: you play black”) |
| `♟3F2A yes` / `♟3F2A no` | accept / decline (or cancel your own invitation before the first move) |
| `♟3F2A 5 g1f3 3. Nf3` | move: half-move number, the move in UCI (`e7e8q` — promotion), notation for a human |
| `♟3F2A draw?` / `♟3F2A draw` | offer a draw / accept the offered draw |
| `♟3F2A resign` | resign |

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

- M9: a pop-up message “Heltec V4: Nf3, your move” (on any page except
  the lock screen), a sound, a “waiting for move” badge on the tile. While the screen
  is locked, the sound and the badge announce the move.
- Heltec: a “Chess” window on the OLED with the same text, three LED flashes, the screen
  wakes up; a click closes the window.
- Web page: a pop-up message about the opponent's move, an invitation, a draw offer,
  the result and an undelivered command; the number of games waiting for a move in the
  tab title and on the tile; optionally a sound (a button on the “Chess” page) and vibration
  where the browser supports it. System browser notifications (Notification API)
  are not available at `http://192.168.4.1`: browsers allow them only over HTTPS.
  The page reports a move while it is open.

## Storage

`/meshmesh/chess.bin` in the internal LittleFS: the `MMC1` header, version, games
(opponent, number, color, state, the last sent command and its status,
the list of moves) and a CRC-32. It is written to `chess.new`, the previous copy is renamed
to `chess.old`, then the new one to `chess.bin`; if the main file is corrupted when read,
`chess.old` is used. On load the moves are replayed by the rules; a game with an
illegal move is discarded. A command that had no ACK before the restart
is considered undelivered and is retried automatically. The SD card and chat history are not touched.

## Checks

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
