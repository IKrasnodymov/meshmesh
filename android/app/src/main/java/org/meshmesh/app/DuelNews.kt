package org.meshmesh.app

import org.json.JSONArray
import org.json.JSONObject

/**
 * Checkers and sea battle news for notifications (/api/duel "games"), compared with the previous list as the web
 * page does (duelNews in web/index.html, the same texts): an invitation, the other side's move or shot, an accepted
 * invitation, a draw offer, the end of a game. One note per game; the title is the game's name. Their hits that
 * let them shoot again wait for the shot that gives the turn back.
 */
object DuelNews {
    data class Note(val game: String, val title: String, val text: String)

    fun between(before: JSONArray, after: JSONArray): List<Note> {
        val old = HashMap<String, JSONObject>()
        for (i in 0 until before.length()) before.getJSONObject(i).let { old[it.optString("id")] = it }
        val notes = ArrayList<Note>()
        for (i in 0 until after.length()) {
            val g = after.getJSONObject(i)
            val id = g.optString("id")
            val n = g.optString("name").ifBlank { I18n.t("Соперник") }
            val sea = g.optString("kind") == "sea"
            val o = old[id]
            val state = g.optString("state")
            val text = when {
                o == null -> if (state == "invited") if (sea) I18n.t("{0} зовёт вас в морской бой", n) else I18n.t("{0} зовёт вас в шашки", n) else null
                sea && g.optString("last_in").let { it.isNotEmpty() && it != o.optString("last_in") } -> {
                    val c = g.optString("last_in").uppercase()
                    // A hit lets them shoot on: a notification once the turn comes back (or the game ends).
                    if (!g.optBoolean("my_turn") && state != "over") null else when {
                        state == "over" && g.optString("result") == "lost" -> I18n.t("{0}: {1} — флот потоплен, вы проиграли", n, c)
                        g.optInt("ships_left") < o.optInt("ships_left") -> I18n.t("{0}: {1} — ваш корабль потоплен", n, c)
                        g.optBoolean("my_turn") -> I18n.t("{0}: {1} — мимо, ваш выстрел", n, c)
                        else -> I18n.t("{0}: {1} — попадание", n, c)
                    }
                }
                !sea && g.optInt("plies") > o.optInt("plies") && lastByThem(g) ->
                    "$n: ${g.optString("last")}" + if (state == "over") " — " + state(g).lowercase() else I18n.t(" — ваш ход")
                o.optString("state") != "over" && state == "over" -> "$n: " + state(g).lowercase()
                o.optString("state") == "inviting" && state == "playing" ->
                    I18n.t("{0} принял вызов", n) + if (g.optBoolean("my_turn")) if (sea) I18n.t(" — ваш выстрел") else I18n.t(" — ваш ход") else ""
                !sea && o.optString("draw_offer") != "theirs" && g.optString("draw_offer") == "theirs" && state == "playing" -> I18n.t("{0} предлагает ничью", n)
                else -> null
            } ?: continue
            notes += Note(id, if (sea) I18n.t("Морской бой") else I18n.t("Шашки"), text)
        }
        return notes
    }

    private fun lastByThem(g: JSONObject): Boolean {
        val plies = g.optInt("plies")
        return plies > 0 && (if (plies % 2 == 1) "white" else "black") != g.optString("color")
    }

    /** A finished game, as duelState on the page. */
    fun state(g: JSONObject): String {
        val reason = g.optString("reason")
        if (reason == "declined") return I18n.t("Вызов отклонён")
        if (reason == "cancelled") return I18n.t("Партия отменена")
        val r = g.optString("result")
        val sea = g.optString("kind") == "sea"
        val why = when (reason) {
            "rules" -> if (sea) (if (r == "won") I18n.t(": флот соперника потоплен") else I18n.t(": ваш флот потоплен"))
                else if (r == "won") I18n.t(": у соперника нет ходов") else I18n.t(": у вас нет ходов")
            "resigned" -> if (r == "won") I18n.t(": соперник сдался") else I18n.t(": вы сдались")
            "agreed" -> I18n.t(" по соглашению")
            "quiet" -> I18n.t(": 15 ходов дамками без взятий")
            else -> ""
        }
        return (if (r == "won") I18n.t("Победа") else if (r == "lost") I18n.t("Поражение") else I18n.t("Ничья")) + why
    }
}
