# Dice (the DIC3R dice roller)

Dice for tabletop games: the DIC3R app brought into MeshMesh. Three modes, as in the app: **RPG** (a dice
pool and formulas), **Warhammer** (a grid of dice with a success threshold) and **counters** (life, wounds,
resources). Saved rolls and counters belong to a character. The device keeps it all, so the screen, the web
page and the Android app show the same rolls: a roll from the phone appears on the board's screen at once and
the other way round. The hardware random number generator rolls the dice, without modulo bias. Rolls are not
sent over the radio.

On every board: a "Dice" tile on the M9 and T-Deck, a screen after "Pet" on the one-button boards, the GAT562
and the T114. It works in every role, the repeater and room server included.

Code: `src/Dice.cpp`, `include/Dice.h` (rolls, formulas, storage, the `dice` command), `src/UiDice.inc` (the
320×240 screen), `src/UiDiceCompact.inc` (the 128×64 OLED and the menus of the one-button boards), the page in
`src/UiHires.inc` (T114), the die shapes — `drawDieShape` in `src/UiIcons.cpp`. The state is the file
`/meshmesh/dice.bin` in LittleFS (about 4.5 KB, with a checksum); the last eight rolls and the grid values
live in memory only.

## Modes

**RPG.** The pool: the number of dice (1–99), the die (d4, d6, d8, d10, d12, d20, d66, d%) and a modifier
(−99…+99), such as `2d6+1`. Any formula: terms joined by `+` and `−`, each `NdX` (N up to 99, X from 2 to
1000), `dX`, `d%`, `d66` or a number, up to eight terms: `3d8+2d4-1`, `d20+5`. The Russian notation `2к6`
(and `д`) is understood too. The result draws each die in its shape (d4 a triangle, d6 with pips, d8 a
diamond, d10 a kite, d12 a pentagon, d20 a hexagon), a maximum green, a one red, and the sum. d66 and d% show
each die on its own and, as in the app, without a sum (d% from `01` to `00`). The last eight rolls are kept.

**Warhammer.** A grid of 20 dice (up to 50 with "+5"): choose how many of the first dice to roll, and the
roll lays them out from high to low. With the threshold on (`4+`) dice at or over it are green, under it red,
and the successes are counted; without it, the sum. The grid's die is chosen apart from the RPG pool.

**Counters.** Up to six per character: a name, a colour of the app's palette (15 colours), a value (20 for a
new one), buttons −5, −1, +1, +5. Counters are written two seconds after the last press, and at once when the board is switched off.

**Characters.** The "General" set and up to five characters, each with up to ten saved rolls (name, formula,
colour) and its own counters. A new character gets one counter, as in the app. The general set stays.

## Controls

**M9 and T-Deck.** Tabs on top: `M` (or `1`–`3`, a tap on a tab) changes the mode, `P` opens the characters.
RPG: `OK` rolls the pool, `<` `>` the die, `^` `v` the number of dice, `+`/`−` the modifier, `R` back to one
die without a modifier, `E` any formula, `S` the saved rolls (`OK` roll, `A` add: the formula, then the name,
`F` formula, `N` name, `C` colour, `DEL` twice deletes), `X` clears the history. Warhammer: the arrows choose
the number of dice (up/down by ten), `OK` rolls, `A` adds five dice, `R` resets the grid, `D` changes the die,
`T` switches the threshold, `+`/`−` move it. Counters: `<` `>` choose, `^` `v` and `OK` ±1, `+`/`−` ±5, `A`
new, `N` name, `C` colour, `DEL` twice deletes. On the T-Deck all of it also takes taps: tabs, the pool's die,
the roll card, grid cells, the ±1/±5 buttons.

**One-button boards, GAT562, T114.** A hold (OK on the joystick) opens the menu with the roll first: hold,
hold rolls. The RPG menu has the saved roll and the next one, the number of dice, the die, the modifier;
Warhammer "Dice +1", "Dice +5", the die, the threshold, "Add 5 dice"; counters "Count with button" (±1 on the
joystick instead), ±5, next, new; each has the character (when there are several) and the mode. Joystick up/down
change the number of dice, the chosen grid dice or the counter. Names and formulas are easier to set on the web
page or in the app.

**Count with button** (one-button boards and the T114). The item opens counting for the chosen counter: click +1,
hold (1.2 s) −1, keep holding to 5 s to leave (that hold's −1 is taken back, the value returns). While counting,
a press with the screen off counts at once; messages and pop-ups close with a click as usual. Counting also ends
when the web page changes the dice mode or deletes the counters.

**Web page and app.** The "Кости" (Dice) section: mode tabs, the character, the pool with ±/R buttons, the
eight dice, a formula field, saved rolls (a tap rolls, ✎ edits the name, formula, colour or deletes), the
Warhammer grid by taps, the threshold slider, counter cards. The page polls the device every 2 s (3 s in the
app) and sends commands one at a time.

## The `dice` command

USB, Bluetooth and `/api/command`; `GET /api/dice` is the same as `dice`.

| Command | Action |
|---|---|
| `dice` | the state (JSON): mode, characters, pool, saved rolls, counters, results, grid |
| `dice roll [FORMULA]` | roll a formula or the pool |
| `dice pool N TYPE [MOD]` | the pool: `dice pool 3 d8 -2` |
| `dice mode rpg\|grid\|counters`, `dice clear` | the mode; clear the history |
| `dice saved add FORMULA NAME`, `edit I FORMULA NAME`, `del I`, `color I C`, `roll I` | saved rolls |
| `dice grid choose N\|roll\|row\|reset\|type T\|threshold off\|N` | the Warhammer grid |
| `dice counter add [NAME]`, `I +N`, `I -N`, `I set N`, `I name NAME`, `I color C`, `I del` | counters |
| `dice char add NAME`, `use I`, `name NAME`, `del` | characters (`name` and `del`: the current one) |

Actions answer `OK … {JSON}`, errors `ERR …`. In the JSON each part of a roll is one number:
`value×4+kind` (0 die, 1 number, 2 d66, 3 d%), negative when subtracted.

## Checked and not checked

Checked on a computer: the formula rules, the ranges of d6/d%/d66, the grid's order, the success count, the
commands and the JSON size — the `tools/ui_preview` scenario (`dice checks passed`), the M9, T-Deck (with
taps), Heltec, GAT562 and T114 screens in the preview; the web section in a browser with a mock API that runs
the same `Dice.cpp`; the app test `diceAreReadFreshAndTheirActionsGoAsCommands`. Results on devices are in
[verification.md](verification.md).
