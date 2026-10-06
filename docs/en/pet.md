# Mesh pet

A Tamagotchi-like pixel creature that lives on the device and feeds on the radio. It is on every board:
the "Pet" tile on the M9 and T-Deck, its own screen after "Chess" on the one-button boards and the GAT562.
The Heltec V4/V3, Wireless Tracker, T-Beam and other boards with a 128×64 OLED draw it in outline, like
the LCD of a keychain pet; the T114 draws it in colour at its native 240×135. The pet also lives in the
repeater and room server modes, where the relayed traffic feeds it.

Code: `src/Pet.cpp`, `include/Pet.h` (life, food, saving, 16×16 sprites), `src/UiPet.inc` (320×240
screen), `src/UiPetCompact.inc` (128×64 OLED), its page in `src/UiHires.inc` (T114). The state is the
file `/meshmesh/pet.bin` in LittleFS (about 450 bytes with a checksum), not NVS.

## What it eats

| Event | Gives |
|---|---|
| A received LoRa packet | food +0.2 %, experience +1; the antenna on its head lights up |
| A packet relayed for others | food +0.2 %, joy +0.2 %, experience +1 |
| Every 25 packets (received or relayed) | a snack (9 at most) |
| An incoming message (direct or channel) | food +1.5 %, joy +4 %, experience +5 |
| A delivered message (ACK) | a snack, joy +4 %, experience +10 |
| A node heard for the first time in its life | joy +15 %, experience +30 ("New friend: …") |
| A chess win (by the rating book) | joy +20 %, experience +50 |
| A walk: the M9's IMU feels the device carried | joy +6 %, experience +3 |
| Petting (at most every 20 s) | joy +10 %, experience +1 |
| Feeding a snack | food +30 % |
| Healing (a snack, at most every 5 min, health below 80 %) | health +30 % |
| Every 6 hours of running | a crumb from the air: a snack |

Time runs only while the device is on: a switched-off pet sleeps and does not get hungry. Food drops
4 % an hour, joy 3 % an hour (1.2 % an hour in the server modes, where nobody plays with it). At night
by the local clock (23:00–7:00, when the clock is set) it sleeps: food drops 1.5 % an hour and joy not at all.

## Growing up

The egg hatches after 15 minutes of running. Then the stage follows the level, but no faster than the age:

| Stage | Level | Age at least |
|---|---|---|
| Baby | 1–2 | — |
| Kid | 3–5 | 2 h |
| Teen | 6–9 | 1 day |
| Adult | 10+ | 3 days |

Level n starts at 25·n·(n−1) experience: level 2 at 50, level 3 at 150, level 10 at 2250. The species
(body colour) and the name of a new egg come from the node key; the name can be changed.

## Death

Health grows 2.5 % an hour while food and joy are above 30 %. When food or joy is at zero, health loses
1.5 % an hour for each of them. Silent air with no care at all kills it in about 2.5 days of running
(a day until the bowl is empty, then hunger and boredom together); hunger alone with good spirits takes
almost 4 days.

At zero health the pet dies "of hunger" (food at zero) or "of loneliness". A grave with its name, age and
cause stays in its place; the last three pets are remembered. OK (hold on the OLED) starts a new egg of
the next generation.

Death can be switched off: "Death: off" (key D on the M9, a menu item on the OLED, `pet mortal off`).
Health then stays at 1 % or more: the pet is ill and sad, but does not die.

In the repeater and room modes there is no owner nearby, so a hungry pet eats a snack itself.

## Screens

**M9 and T-Deck.** The "Pet" tile on the home screen (yellow when it needs care); in the repeater and
room modes the P key opens it. On the left the creature on its card with a speech bubble, on the right
its name, stage, level, experience and the Food, Joy and Health bars. Keys: OK pets it (a tap on the
card on the T-Deck), F feeds it, H heals it, N renames it, D switches death on or off. The locked
screen shows a small pet under the clock when no game or unread message takes the place.

**128×64 OLED (Heltec and other one-button boards).** The screen after "Chess": the creature on the
left, its mood and three bars with signs (bowl, heart, cross) on the right. Holding the button opens
the menu: "Pet", "Feed", "Heal", "Death: on/off"; at a grave, "New egg". On the GAT562 the joystick
centre does the same.

**T114.** The same screen and menu in colour: a card with the creature, a speech bubble, bars with icons.

It says things like "New friend: …", "A message, how tasty", "Hungry: the air is empty", "Write to
someone?", and moves its mouth while talking. Moods: happy, calm, hungry, lonely, asleep, ill, eating.

## USB

```
pet                       the state as JSON (food, joy, health 0–1000, level, experience, counters, graves)
pet cuddle|feed|heal      pet it, feed it, heal it
pet egg                   a new egg (only after death)
pet mortal on|off         death on or off
pet name NAME             the name, 1–15 UTF-8 bytes
pet skip SECONDS          run its clock forward (up to 14 days) to check growing up and death
```

## Not done yet

- The web page and the Android app do not show the pet yet (only USB `pet`).
- Meetings and exchange between pets of different devices (a BLE "boop", visits over LoRa, a signed
  album of meetings) and a repeater answering `!pet` from the mesh are the next step.
- The boards have no NFC; BLE and LoRa are meant to take the place of tapping a tag.
- Boards without a screen (XIAO, a T-Beam without OLED) keep a pet, but only USB shows it.

## Checks

In the screen emulator (`tools/ui_preview/build.sh m9|heltec|gat562|t114`) the scenario opens the pet
page, shoots the egg, a happy, a hungry and an ill pet and the grave, and checks death at zero health,
a new egg with the old pet remembered, hatching after 15 minutes and no death when death is off.
Checks on the boards are recorded in [verification.md](verification.md).
