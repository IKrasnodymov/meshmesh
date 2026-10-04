package org.meshmesh.app

import org.json.JSONArray
import org.json.JSONObject

/**
 * Chess news for notifications: the board's game list (/api/chess "games") compared with the previous
 * one, as the web page does, so the text does not depend on the board's menu language. One note per game.
 */
object ChessNews {
    data class Note(val game: String, val title: String, val text: String)

    fun between(before: JSONArray, after: JSONArray): List<Note> {
        val old = HashMap<String, JSONObject>()
        for (i in 0 until before.length()) before.getJSONObject(i).let { old[it.optString("id")] = it }
        val notes = ArrayList<Note>()
        for (i in 0 until after.length()) {
            val g = after.getJSONObject(i)
            val id = g.optString("id")
            val name = g.optString("name").ifBlank { "Соперник" }
            val o = old[id]
            val text = when {
                o == null -> if (g.optString("state") == "invited") state(g) else null
                g.optInt("plies") > o.optInt("plies") && lastByThem(g) ->
                    ruSan(g.optString("last_san")) + " — " + state(g).lowercase()
                o.optString("state") != "over" && g.optString("state") == "over" -> state(g)
                o.optString("state") == "inviting" && g.optString("state") == "playing" -> "Вызов принят" + if (g.optBoolean("my_turn")) " — ваш ход" else ""
                o.optString("draw_offer") != "theirs" && g.optString("draw_offer") == "theirs" && g.optString("state") == "playing" -> "Предлагает ничью"
                else -> null
            } ?: continue
            notes += Note(id, name, text)
        }
        return notes
    }

    /** K Q R B N in Russian notation (Кр Ф Л С К), as on the page. */
    fun ruSan(san: String): String {
        val names = mapOf('K' to "Кр", 'Q' to "Ф", 'R' to "Л", 'B' to "С", 'N' to "К")
        val out = StringBuilder()
        san.forEachIndexed { i, c -> out.append(if ((i == 0 || san[i - 1] == '=') && c in names) names[c] else c) }
        return out.toString()
    }

    private fun lastByThem(g: JSONObject): Boolean {
        val plies = g.optInt("plies")
        return plies > 0 && (if (plies % 2 == 1) "white" else "black") != g.optString("color")
    }

    fun state(g: JSONObject): String {
        when (g.optString("state")) {
            "inviting" -> return "Вызов отправлен, ждём ответа"
            "invited" -> return "Вызывает вас: вы играете " + if (g.optString("color") == "white") "белыми" else "чёрными"
            "playing" -> return if (g.optString("draw_offer") == "theirs") "Предлагает ничью"
                else if (g.optBoolean("my_turn")) (if (g.optBoolean("check")) "Ваш ход: шах!" else "Ваш ход") else "Ход соперника"
        }
        val reason = g.optString("reason")
        if (reason == "declined") return "Вызов отклонён"
        if (reason == "cancelled") return "Партия отменена"
        val result = g.optString("result")
        val won = result.isNotEmpty() && result == g.optString("color")
        val lost = result.isNotEmpty() && result != "draw" && !won
        val why = mapOf("mate" to ": мат", "resigned" to if (won) ": соперник сдался" else ": вы сдались", "stalemate" to ": пат",
            "repetition" to ": троекратное повторение", "fifty" to ": 50 ходов без взятий", "material" to ": мало фигур для мата",
            "too_long" to ": предел ходов", "agreed" to " по соглашению")[reason] ?: ""
        return (if (won) "Победа" else if (lost) "Поражение" else "Ничья") + why
    }
}
