package org.meshmesh.app

import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Test

class DuelNewsTest {
    private fun game(id: String, kind: String, vararg fields: Pair<String, Any>) =
        JSONObject().put("id", id).put("kind", kind).put("name", "Heltec V4").apply { fields.forEach { put(it.first, it.second) } }
    private fun list(vararg g: JSONObject) = JSONArray().apply { g.forEach { put(it) } }

    @Test fun checkersMoveOfTheOtherSideOnly() {
        val before = list(game("41A8", "checkers", "state" to "playing", "color" to "white", "plies" to 4, "my_turn" to true))
        val mine = list(game("41A8", "checkers", "state" to "playing", "color" to "white", "plies" to 5, "my_turn" to false, "last" to "c3-d4"))
        assertEquals(emptyList<DuelNews.Note>(), DuelNews.between(before, mine))
        val theirs = list(game("41A8", "checkers", "state" to "playing", "color" to "white", "plies" to 6, "my_turn" to true, "last" to "f6:d4"))
        assertEquals(listOf(DuelNews.Note("41A8", "Шашки", "Heltec V4: f6:d4 — ваш ход")), DuelNews.between(mine, theirs))
        val lost = list(game("41A8", "checkers", "state" to "over", "color" to "white", "plies" to 6, "result" to "lost", "reason" to "rules", "last" to "f6:d4"))
        assertEquals("Heltec V4: f6:d4 — поражение: у вас нет ходов", DuelNews.between(mine, lost).single().text)
    }

    @Test fun seaShotsAndInvitation() {
        assertEquals("Heltec V4 зовёт вас в морской бой", DuelNews.between(list(), list(game("4BC8", "sea", "state" to "invited"))).single().text)
        val before = list(game("4BC8", "sea", "state" to "playing", "ships_left" to 10, "last_in" to "a1", "my_turn" to false))
        val miss = list(game("4BC8", "sea", "state" to "playing", "ships_left" to 10, "last_in" to "e5", "my_turn" to true))
        assertEquals("Heltec V4: E5 — мимо, ваш выстрел", DuelNews.between(before, miss).single().text)
        // A sunk ship lets them shoot on: no notification until the turn comes back.
        val sunk = list(game("4BC8", "sea", "state" to "playing", "ships_left" to 9, "last_in" to "e5", "my_turn" to false))
        assertEquals(emptyList<DuelNews.Note>(), DuelNews.between(before, sunk))
        val lost = list(game("4BC8", "sea", "state" to "over", "result" to "lost", "ships_left" to 0, "last_in" to "e5", "my_turn" to false))
        assertEquals("Heltec V4: E5 — флот потоплен, вы проиграли", DuelNews.between(before, lost).single().text)
        // My own shot answered is not news for a notification.
        val answered = list(game("4BC8", "sea", "state" to "playing", "ships_left" to 10, "last_in" to "a1", "last_out" to "c3", "my_turn" to true))
        assertEquals(emptyList<DuelNews.Note>(), DuelNews.between(before, answered))
    }
}
