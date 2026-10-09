package org.meshmesh.app

import org.json.JSONObject
import java.util.Calendar

/**
 * Notification settings of the page (web/index.html "alerts", passed by MeshNative.setAlerts) and its rules:
 * direct, channels and Public each "all", "mention" or "off"; a conversation may override its kind. A mention is
 * the "@[name]" of MeshCore apps or a keyword, without regard to case.
 */
class Alerts(json: String?) {
    private val o = runCatching { JSONObject(json ?: "{}") }.getOrDefault(JSONObject())
    private val per = o.optJSONObject("per") ?: JSONObject()
    val words = o.optJSONArray("words")?.let { a -> (0 until a.length()).map { a.optString(it) }.filter { it.isNotBlank() } } ?: emptyList()
    val chess = o.optBoolean("chess", true)
    val nodes = o.optBoolean("nodes", false)
    val lost = o.optBoolean("lost", true)
    val sound = o.optBoolean("sound", true)
    private val quiet = o.optBoolean("quiet", false)
    private val from = minutes(o.optString("from", "23:00"))
    private val to = minutes(o.optString("to", "07:00"))

    fun mode(conversation: String): String =
        per.optString(conversation).ifEmpty { o.optString(kind(conversation)).ifEmpty { if (kind(conversation) == "pub") "mention" else "all" } }

    fun mentioned(text: String, myName: String): Boolean {
        val t = text.lowercase()
        return myName.isNotEmpty() && "@[${myName.lowercase()}]" in t || words.any { it.lowercase() in t }
    }

    /** "mention", "message" or null (no notification) for a received message in this conversation. */
    fun forMessage(conversation: String, text: String, myName: String): String? {
        val m = mode(conversation)
        return if (m == "off") null else if (mentioned(text, myName)) "mention" else if (m == "all") "message" else null
    }

    /** Quiet hours by the phone's clock: notifications come without sound and vibration. */
    fun quietAt(c: Calendar = Calendar.getInstance()): Boolean {
        if (!quiet) return false
        val n = c.get(Calendar.HOUR_OF_DAY) * 60 + c.get(Calendar.MINUTE)
        return if (from <= to) n in from until to else n >= from || n < to
    }

    companion object {
        /** Public is "ALL", channels "FF" and 14 hex digits (node IDs never start with FF), as on the page. */
        fun isChannel(id: String) = id == "ALL" || Regex("^FF[0-9A-F]{14}$").matches(id)
        fun kind(id: String) = if (id == "ALL") "pub" else if (isChannel(id)) "chan" else "dm"
        /** The conversation of a received message: its channel, else its sender. */
        fun conversation(m: JSONObject): String {
            val d = m.optString("destination")
            return if (d == "FFFFFFFFFFFFFFFF" || d == "ALL") "ALL" else if (isChannel(d)) d else m.optString("source")
        }
        private fun minutes(s: String): Int {
            val p = s.split(":")
            return (p.getOrNull(0)?.toIntOrNull() ?: 0) * 60 + (p.getOrNull(1)?.toIntOrNull() ?: 0)
        }
    }
}
