package org.meshmesh.app

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.Calendar

class AlertsTest {
    private val chan = "FF0123456789ABCD"
    private val node = "1A2B3C4D5E6F7081"

    @Test fun withoutSettingsPublicNeedsAMention() {
        val a = Alerts(null)
        assertEquals("message", a.forMessage(node, "привет", "Igor"))
        assertEquals("message", a.forMessage(chan, "всем", "Igor"))
        assertEquals(null, a.forMessage("ALL", "всем", "Igor"))
        assertEquals("mention", a.forMessage("ALL", "@[igor] ты тут?", "Igor"))
    }

    @Test fun conversationOverridesItsKindAndOffSilencesMentions() {
        val a = Alerts("""{"chan":"all","pub":"all","per":{"$chan":"off","ALL":"mention"},"words":["Казань"]}""")
        assertEquals(null, a.forMessage(chan, "@[Igor] смотри", "Igor"))
        assertEquals(null, a.forMessage("ALL", "просто текст", "Igor"))
        assertEquals("mention", a.forMessage("ALL", "кто едет в казань?", "Igor"))
        assertEquals("message", a.forMessage("FF00000000000001", "текст", "Igor"))
    }

    @Test fun conversationOfAMessage() {
        assertEquals("ALL", Alerts.conversation(JSONObject().put("source", node).put("destination", "FFFFFFFFFFFFFFFF")))
        assertEquals(chan, Alerts.conversation(JSONObject().put("source", node).put("destination", chan)))
        assertEquals(node, Alerts.conversation(JSONObject().put("source", node).put("destination", "0000000000000001")))
    }

    @Test fun quietHoursAcrossMidnight() {
        val a = Alerts("""{"quiet":true,"from":"23:00","to":"07:00"}""")
        val at = { h: Int, m: Int -> Calendar.getInstance().apply { set(Calendar.HOUR_OF_DAY, h); set(Calendar.MINUTE, m) } }
        assertTrue(a.quietAt(at(23, 0)))
        assertTrue(a.quietAt(at(3, 30)))
        assertFalse(a.quietAt(at(7, 0)))
        assertFalse(a.quietAt(at(12, 0)))
        assertFalse(Alerts("""{"quiet":false}""").quietAt(at(3, 0)))
    }
}
