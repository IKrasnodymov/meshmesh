package org.meshmesh.app.api

import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Test

class MessageHistoryTest {
    private fun message(id: Int, destination: String = "ALL", time: Int = 0) = JSONObject()
        .put("source", "1234").put("destination", destination).put("session", 7).put("id", id)
        .put("text", "text $id").put("time", time).put("status", 0)
    @Test fun shortBoardSnapshotDoesNotEraseThePhoneArchive() {
        val h = MessageHistory(); h.merge(JSONArray((0..31).map { message(it) }))
        h.merge(JSONArray((24..39).map { message(it) }))
        assertEquals(40, h.messages().length())
        assertEquals(40, MessageHistory(h.saved()).messages().length())
    }
    @Test fun timeAckAndRepeatUpdatesKeepOneMessage() {
        val h = MessageHistory(); h.merge(JSONArray().put(message(1)))
        h.merge(JSONArray().put(message(1, time = 123).put("status", 3).put("heard", 4)))
        h.merge(JSONArray().put(message(1)))
        assertEquals(1, h.messages().length())
        assertEquals(123, h.messages().getJSONObject(0).getInt("time"))
        assertEquals(3, h.messages().getJSONObject(0).getInt("status"))
        assertEquals(4, h.messages().getJSONObject(0).getInt("heard"))
    }
    @Test fun repeatedBoardSnapshotDoesNotResurrectPrunedOlderMessages() {
        val h = MessageHistory()
        h.policies(JSONArray().put(JSONObject().put("id", "ALL").put("policy", JSONObject().put("app_limit", 8))))
        val snapshot = JSONArray((0..15).map { message(it) }); h.merge(snapshot)
        repeat(5) { h.merge(snapshot) }
        assertEquals(8, h.messages().length())
        assertEquals(8, h.messages().getJSONObject(0).getInt("id"))
        val restored = MessageHistory(h.saved()); restored.merge(snapshot)
        assertEquals(8, restored.messages().getJSONObject(0).getInt("id"))
    }
    @Test fun changingALowPriorityLimitTrimsOnlyThatChannel() {
        val h = MessageHistory(); h.merge(JSONArray((0..40).flatMap { listOf(message(it), message(it, "FF0123456789ABCD")) }))
        h.policies(JSONArray().put(JSONObject().put("id", "ALL").put("policy", JSONObject().put("priority", 0).put("app_limit", 8))))
        assertEquals(49, h.messages().length())
        assertEquals(8, (0 until h.messages().length()).count { h.messages().getJSONObject(it).getString("destination") == "ALL" })
    }
}
