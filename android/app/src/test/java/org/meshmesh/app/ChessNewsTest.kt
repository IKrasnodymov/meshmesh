package org.meshmesh.app

import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Test

class ChessNewsTest {
    private fun game(id: String, vararg fields: Pair<String, Any>) = JSONObject().put("id", id).put("name", "Heltec V4").apply { fields.forEach { put(it.first, it.second) } }
    private fun list(vararg g: JSONObject) = JSONArray().apply { g.forEach { put(it) } }

    @Test fun opponentMoveIsNewsMineIsNot() {
        val before = list(game("3F2A", "state" to "playing", "color" to "white", "plies" to 2, "my_turn" to true))
        val mine = list(game("3F2A", "state" to "playing", "color" to "white", "plies" to 3, "my_turn" to false, "last_san" to "Nf3"))
        assertEquals(emptyList<ChessNews.Note>(), ChessNews.between(before, mine))
        val theirs = list(game("3F2A", "state" to "playing", "color" to "white", "plies" to 4, "my_turn" to true, "last_san" to "Nc6"))
        assertEquals(listOf(ChessNews.Note("3F2A", "Heltec V4", "Кc6 — ваш ход")), ChessNews.between(mine, theirs))
    }

    @Test fun challengeMateAndDrawOffer() {
        val invite = list(game("0001", "state" to "invited", "color" to "black", "plies" to 0))
        assertEquals("Вызывает вас: вы играете чёрными", ChessNews.between(list(), invite).single().text)
        val before = list(game("0002", "state" to "playing", "color" to "black", "plies" to 6))
        val mated = list(game("0002", "state" to "over", "color" to "black", "plies" to 7, "result" to "white", "reason" to "mate", "last_san" to "Qxf7#"))
        assertEquals("Фxf7# — поражение: мат", ChessNews.between(before, mated).single().text)
        val offer = list(game("0002", "state" to "playing", "color" to "black", "plies" to 6, "draw_offer" to "theirs"))
        assertEquals("Предлагает ничью", ChessNews.between(before, offer).single().text)
    }

    @Test fun promotionKeepsPieceLetterInRussian() = assertEquals("e8=Ф+", ChessNews.ruSan("e8=Q+"))
}
