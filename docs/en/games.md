# Checkers, sea battle and app folders

Since 0.33.0 MeshMesh plays two more games with your contacts — **Russian draughts** (checkers) and **sea battle**.
Like chess (`docs/en/chess.md`) they are games between two MeshCore nodes over acknowledged direct messages: they pass
any repeaters and read as plain text in a stock MeshCore app. No rating, points or tournaments — just play with
someone. All games sit in a Games folder of the main menu; folders can be created, renamed, hidden and filled with apps.

Code: `src/Checkers.cpp` (draughts rules), `src/Sea.cpp` (field, fleet, answers), `src/Duel.cpp` (games of both kinds,
commands, resending, storage in `/meshmesh/duels.0|1`), screens — `src/UiDuel.inc` (M9, T-Deck),
`src/UiDuelCompact.inc` (128×64 and T114), the web page and the app — the Checkers and Sea battle sections of
`web/index.html`; folders — `src/Apps.cpp`. Up to six games of both kinds run at once (chess has its own six).

## Checkers

Russian draughts: an 8×8 board, dark squares, White moves first from a1–h2 (the challenger picks White, Black or
random). A man moves forward and captures forward and backward; a king moves and captures any distance along a
diagonal. Capturing is compulsory; among several captures any one may be chosen (not necessarily the longest), and a
capture once started goes on to the end. A man reaching the last row during a capture becomes a king at once and goes
on capturing as a king. Captured men leave the board after the move and block it until then (the "Turkish strike": a
man is never jumped twice). A king capturing at a distance lands on any free square behind the captured man — but only
on one from which it can capture again, when there is such.

The game ends when the side to move has no move (no men left or all blocked) — it loses; after 15 moves of each side
with kings only and no capture — a draw (as in the Russian rules); by an agreed draw; by resignation. Threefold
repetition is not tracked.

A move is written with `-` between squares for a plain move (`c3-d4`) and `:` for a capture with every stop
(`c3:e5:c7`). Typed by hand, the start and the end are enough (`c3:c7`) when only one path joins them.

## Sea battle

A 10×10 field, columns a–j, rows 1–10. The fleet: one four-deck ship, two three-deck, three two-deck and four
one-deck ships; ships do not touch, not even at a corner. The placement is random; until anyone has fired at your
field it can be shuffled as often as you like (the other player cannot see it and it does not matter to them). The
challenger fires first. A hit or a sunk ship gives another shot, a miss passes the turn. The cells around a sunk ship
are marked as water: no need to fire there.

The defender's device answers each shot itself, from its own placement. When the last ship sinks, the winner sends its
placement: the loser sees where the ships it did not find stood, and the device checks it against every answer it got;
if the answers contradict it, the screen and the page warn "the fleet does not match the answers".

## Messages

Each command is an ordinary MeshCore direct message: `🏁` (U+1F3C1, checkers) or `🚢` (U+1F6A2, sea battle), four hex
digits of the game number, a space and the command. Text after the known fields is not read.

| Text | Meaning |
|---|---|
| `🏁3F2A new w · MeshMesh checkers: you play Black` | checkers challenge; `w`/`b` is the challenger's colour |
| `🚢3F2A new · MeshMesh sea battle` | sea battle challenge; the challenger fires first |
| `🏁3F2A yes` / `🏁3F2A no` | accept / decline (or cancel the game before the first move) |
| `🏁3F2A 5 c3:e5:c7` | a move: the half-move number and the move |
| `🏁3F2A draw?` / `🏁3F2A draw` / `🏁3F2A resign` | offer a draw / accept it / resign |
| `🚢3F2A 7 e5` | a shot: the shot number (counted for both sides together) and the cell |
| `🚢3F2A 7 = miss` | the answer to shot 7: `miss`, `hit`, `sunk` (a ship sunk), `win` (the whole fleet sunk) |
| `🚢3F2A fleet a1h c3v e5h …` | the winner's placement after the game: the first cell and the direction (`h` along the row, `v` down), one-deck ships without a letter |
| `🚢3F2A resign` | resign |

Only direct messages from the contact of that game are taken; channel messages never count as moves. A move or a shot
is accepted when its number is the next one and it is the other player's turn; a repeat of the last move (a lost ACK)
is skipped silently, a repeated shot gets the same answer again. The other player's next command confirms my previous
one: their move confirms my move, their answer my shot, their next shot my answer; if the answer to my shot was lost
and they are already firing themselves, my shot missed. Firmware without these games (an nRF52 image without them)
shows such messages as an ordinary chat — the person reads the explanation in the challenge text.

Delivery is as in chess: MeshCore makes up to three attempts; without a confirmation the command is resent after 2, 5
and 10 minutes, then every 15 minutes, and at once when the other player is heard again (at most every 2 minutes);
after 24 hours resending stops, "Send again" starts it once more. Games and the unconfirmed command are saved; after a
restart resending begins in a minute.

## Where to play

Challenge from a chat node's card (M9 and T-Deck: "Play" → chess, checkers or sea battle; OLED and T114: "Checkers:
invite", "Sea battle: invite" in the node menu), from a game's list ("New game"), on the web page and in the app. A
challenge, an answer, the other player's move and the result arrive as notifications, like chess news; the screen wakes.

**M9 and T-Deck.** The Checkers and Sea battle tiles are in the Games folder. The list: a new game (contact, and the
colour with ←/→: White, Black, random), then the games — those waiting for you first. Checkers: your side at the
bottom, kings wear a crown, the last move is tinted; OK takes a man that can move (a compulsory capture shows a "You
must capture" strip), its moves are marked with dots, captures with corners, their path and stops; OK on a square plays,
BACK puts the man back or returns to the list. D offers or accepts a draw, X twice resigns, R sends again, N plays again
(colours swapped), DEL declines, cancels or deletes a finished game, H shows the keys. Sea battle: their field large
(arrows aim, OK fires), your fleet small; a shot in flight is a yellow crosshair; S shuffles your fleet while nobody has
fired at it; at the end the winner's fleet, with the cells whose "miss" does not match it outlined in red. T-Deck: a tap
on the board is the cursor plus OK; in sea battle the first tap aims and a second tap on the same cell fires.

**One button (Heltec V4/V3, Wireless Tracker, T-Beam, E213 and others).** As in chess: on your move a click steps
through your men that can move, then "Game menu"; hold takes one; a click then steps through its whole moves ("f2:d4:f6",
captured men crossed out), then "Cancel"; hold plays. Off your move a click goes to the next screen and hold opens the
menu (accept, decline, draw, resign, send again, delete, list). Sea battle without 100 clicks: first the cells next to
a hit ship not yet sunk ("e5: finish"), then the columns where a shot still makes sense; hold on a column fixes it, a
click steps through its cells, hold fires. The menu has "My fleet" and "Shuffle fleet".

**Joystick (GAT562).** Checkers: a cursor, OK takes and plays; if different capture paths lead to the square, ↑/↓ pick
the path. Sea battle: a cursor on their field, OK fires. Holding OK opens the game menu.

**T114.** The same one-button logic drawn in colour: a 128 px board, their field at 120 px and your fleet small on the
right (the T114 image leaves the games out by default — see "nRF52 boards").

**Web page and app.** The Games tile on the home page opens all the board's games. Checkers: the list, a new game
(contact and colour), the board — tap your man, then the square it goes to (its moves are highlighted); draw, resign,
send again. Sea battle: their field large (a tap fires on your turn), your field beside it, "Shuffle fleet" until the
first shot at you. `GET /api/duel` (the list), `GET /api/duel?id=ID` (the board), actions — `POST /api/command` with
`duel …` commands.

**USB.**

| Command | Action |
|---|---|
| `duel` | the games (JSON) |
| `duel invite NODE_ID checkers [w\|b\|r]` / `duel invite NODE_ID sea` | challenge |
| `duel accept\|decline\|draw\|resign\|resend\|remove\|seen\|shuffle ID` | game actions |
| `duel move ID c3-d4` / `duel shoot ID e5` | move / shot |
| `duel show ID` | the whole game: the checkers board with its legal moves, both sea battle fields |

## App folders

The order of the main menu's apps (the `apps` setting, Settings → Apps on the web page and in the app) can now hold
folders. By default every game of the board — chess, checkers, sea battle, pet, dice, solitaire — is in the Games
folder. On the page you can create a folder, rename it, delete it (its apps return to the main menu), hide it with
everything in it, move it, and move an app into another folder or back. Settings never go into a folder and are never
hidden. Up to six folders, not nested.

Format: IDs separated by spaces, `-ID` hides an app, `{NAME id id …}` is a folder (`_` for a space in the name, up to
23 bytes), `-{NAME …}` a hidden folder; a folder named `games` is shown as "Games" in the screen language.
For example: `set {"apps":"chats quick nodes {games chess checkers sea pet dice} {Maps map nav} radar settings"}`.
Apps the text does not name (new in a firmware) take their default place: games go into the `games` folder when there
is one, the rest to the end. On the first start of 0.33.0 an order saved before moves the games into the Games folder
once, where the first of them stood (their order and hidden marks stay).

What a folder looks like on the device:
- **M9 and T-Deck** — a tile with a folder icon, its name, its first apps and the sum of their badges (moves waiting).
  OK or a tap opens the folder's page with the same tiles; BACK returns home to the folder tile. An app opened from a
  folder returns to it on BACK.
- **One button and T114** — the folder is a screen of its own in the page cycle with a list of its apps. A click goes
  to the next screen of the main menu, hold enters (the folder's first app opens); inside, a click steps through the
  folder's apps and after the last one returns to the folder screen; a double click (when enabled) goes to the folder
  screen.
- **Joystick (GAT562)** — ↑/↓ choose an app of the folder, OK opens it, ←/→ step through the folder's apps and its
  screen, BACK returns to the folder screen.

The web page and the app arrange their own sections differently: the Games tile of the home page opens every game of the
board. A limit: over BLE a command is at most 255 bytes, so the page does not save an order longer than 240 bytes
(a Cyrillic letter takes 2 bytes).

## nRF52 boards

In the 1 MB flash of the GAT562, T114, T096 and fakeTec checkers and sea battle do not fit yet together with every
language, so their images (from the site, the app and `tools/nrf52.py package ENV`) are built **without these two
games** (`include/Modules.h`: `MM_NO_CHECKERS` and `MM_NO_SEA` on nRF52 unless `MM_WITH_GAMES` is set). Folders, and
the Games folder with chess, the pet and the dice, are there; a checkers or sea battle challenge reaches such a board as
an ordinary message whose text explains it, and the page and the app hide these games (`status.modules`). The games'
strings stay out of such an image's translation table (`tools/i18n.py`, the `GAMES` group).

To build an image with the games for your board: `tools/nrf52.py package ENV --with-games` — English and Russian only
(a `…-with-games` package, no `lang/`). Sizes in 0.33.0 (the limit is 712704 bytes):

| Board | plain image, Chinese (the largest) | `--with-games`, en+ru |
|---|---|---|
| GAT562 | 691124 | 697656 |
| T096 | 689312 | 695188 |
| fakeTec | 688732 | 694608 |
| T114 | 710072 (2.6 KB free) | does not fit (15.8 KB over); `--with-games --without chess` — 676492 |

For the Chinese T114 to fit together with the folders, 0.33.0 freed about 2.5 KB: the initial values of
`quickSend::state` and of the radio draft are no longer kept in the image (zeroed memory, the values are set at
start), and the T114's large font in images without Russian is the Latin `inr24_mr` with the 10×20 one for the rest.
