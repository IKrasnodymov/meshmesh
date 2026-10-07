package org.meshmesh.app

import kotlinx.coroutines.runBlocking
import org.json.JSONObject
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.meshmesh.app.api.CommandApi
import org.meshmesh.app.api.TileStore
import org.meshmesh.app.link.ByteLink
import org.meshmesh.app.link.GarbledAnswerException
import org.meshmesh.app.link.LineAssembler
import org.meshmesh.app.link.LineTransport
import java.io.IOException
import java.nio.file.Files
import java.util.Base64
import java.util.zip.CRC32

/** A board stand-in: answers each command line with [answer]'s lines, in chunks of [chunk] bytes. */
class FakeBoard(private val chunk: Int = 20, override val maxCommand: Int = 1000, val answer: (String) -> List<String>) : ByteLink {
    override var onBytes: (ByteArray, Int) -> Unit = { _, _ -> }
    override var onClosed: (String) -> Unit = {}
    val commands = ArrayList<String>()
    val writes = ArrayList<ByteArray>()
    override suspend fun write(data: ByteArray) {
        writes += data
        val command = String(data, Charsets.UTF_8).trimEnd('\n')
        commands += command
        val bytes = answer(command).joinToString("") { it + "\n" }.toByteArray(Charsets.UTF_8)
        var at = 0
        while (at < bytes.size) { val n = minOf(chunk, bytes.size - at); onBytes(bytes.copyOfRange(at, at + n), n); at += n }
    }
    override fun close() {}
}

class ProtocolTest {
    @Test fun utf8SplitAcrossNotifications() {
        val text = "Привет, мир ♟ — ok"
        val bytes = (text + "\r\n").toByteArray()
        val a = LineAssembler()
        val lines = bytes.indices.flatMap { i -> a.feed(byteArrayOf(bytes[i])) } // one byte at a time
        assertEquals(listOf(text), lines)
    }

    @Test fun logLinesAreSkippedAndJsonIsTaken() = runBlocking {
        val board = FakeBoard { listOf("READY: USB commands are available; type help", "SELFTEST crypto/UTF-8/tamper PASS", """{"firmware":"MeshMesh 0.3.2"}""") }
        assertEquals("""{"firmware":"MeshMesh 0.3.2"}""", LineTransport(board).exchange("status"))
    }

    @Test fun jsonCutByDriverLogIsReportedAtOnce() = runBlocking {
        // Seen on the M9 when Wi-Fi stops after the radar: the log line lands inside the JSON.
        val board = FakeBoard { listOf("""{"active":false,"scan_ wifi:timeout when WiFi un-init, type=4""", "frames\":0}}") }
        try { LineTransport(board).exchange("radar", 2000); fail("garbled answer accepted") } catch (_: GarbledAnswerException) {}
    }

    @Test fun lateAnswerOfTimedOutCommandIsNotTakenForTheNext() = runBlocking {
        var first = true
        val board = object : ByteLink {
            override var onBytes: (ByteArray, Int) -> Unit = { _, _ -> }
            override var onClosed: (String) -> Unit = {}
            override val maxCommand = 1000
            override suspend fun write(data: ByteArray) {
                val out = if (first) { // "hello" is answered 0.6 s late, after its 0.3 s timeout
                    first = false
                    Thread { Thread.sleep(600); "OK hello queued\n".toByteArray().let { onBytes(it, it.size) } }.start(); return
                } else "OK message queued\n".toByteArray()
                onBytes(out, out.size)
            }
            override fun close() {}
        }
        val transport = LineTransport(board)
        try { transport.exchange("hello", 300); fail() } catch (_: IOException) {}
        assertEquals("OK message queued", transport.exchange("send ALL hi", 3000))
    }

    @Test fun mapChunksFitBleWrites() = runBlocking {
        val board = FakeBoard(maxCommand = 255) { listOf(if (it.startsWith("map chunk ")) "OK map chunk" else "ERR") }
        val api = CommandApi(LineTransport(board), "ble", "test", null)
        val data = ByteArray(2048) { (it * 7).toByte() }
        val reply = api.request("POST", "/api/maps/chunk", JSONObject().put("data", Base64.getEncoder().encodeToString(data)).toString())
        assertEquals(200, reply.status)
        assertTrue(board.writes.all { it.size - 1 <= 255 })
        val sent = board.commands.flatMap { Base64.getDecoder().decode(it.removePrefix("map chunk ")).toList() }
        assertArrayEquals(data, sent.toByteArray())
    }

    @Test fun messageWithLineBreakGoesAsJson() = runBlocking {
        val board = FakeBoard { listOf("OK message queued") }
        val api = CommandApi(LineTransport(board), "usb", "test", null)
        api.request("POST", "/api/send", """{"to":"ALL","text":"две\nстроки"}""")
        api.request("POST", "/api/send", """{"to":"ALL","text":"одна строка"}""")
        assertEquals(listOf("sendjson {\"to\":\"ALL\",\"text\":\"две\\nстроки\"}", "send ALL одна строка"), board.commands)
        assertTrue(board.commands.none { it.contains('\n') })
    }

    @Test fun statusCodesFollowTheDeviceServer() = runBlocking {
        val board = FakeBoard { c -> listOf(when (c) { "chess show 0001" -> "ERR chess"; "set {\"sf\":13}" -> "ERR SF 7..12"; "wifi" -> "OK Wi-Fi off"; else -> "Commands: …" }) }
        val api = CommandApi(LineTransport(board), "usb", "test", null)
        assertEquals(400, api.request("GET", "/api/chess?id=0001", null).status)
        assertEquals(400, api.request("POST", "/api/config", """{"sf":13}""").status)
        assertEquals(200, api.request("POST", "/api/command", """{"command":"wifi"}""").status)
        assertEquals(404, api.request("GET", "/api/unknown", null).status)
    }

    @Test fun olderFirmwareIsNamedNotShownAsHelp() = runBlocking {
        val board = FakeBoard { listOf("Commands: status, config, key, messages, radar") }
        val api = CommandApi(LineTransport(board), "ble", "test", null)
        val reply = api.request("GET", "/api/radar?open=1", null)
        assertEquals(501, reply.status)
        assertTrue(reply.body.contains("Прошивка"))
    }

    @Test fun tileIsAssembledFromPartsAndReusedWhileUnchanged() = runBlocking {
        val body = ByteArray(15000) { (it % 251).toByte() }
        val crc = CRC32().apply { update(body) }.value
        val header = java.nio.ByteBuffer.allocate(24).order(java.nio.ByteOrder.LITTLE_ENDIAN)
            .put("MMT1".toByteArray()).put(14).put(0).put(0).put(0).putInt(9).putInt(5).putInt(body.size).putInt(crc.toInt()).array()
        val file = header + body
        val board = FakeBoard(chunk = 4096) { c ->
            val p = c.split(' ')
            val offset = p[5].toInt(); val length = p[6].toInt()
            val part = file.copyOfRange(offset, minOf(file.size, offset + length))
            listOf("OK tile ${file.size} $offset ${Base64.getEncoder().encodeToString(part)}")
        }
        val store = TileStore(Files.createTempDirectory("tiles").toFile())
        val api = CommandApi(LineTransport(board), "ble", "test", store)
        val first = api.request("GET", "/api/maps/tile?z=14&x=9&y=5", null)
        assertTrue(first.base64)
        assertArrayEquals(file, Base64.getDecoder().decode(first.body))
        assertEquals(3, board.commands.size) // 0, 6144, 12288
        val again = api.request("GET", "/api/maps/tile?z=14&x=9&y=5", null)
        assertArrayEquals(file, Base64.getDecoder().decode(again.body))
        assertEquals(4, board.commands.size) // only the first part, to compare the header
    }

    @Test fun bleListsAreReadAgainWhenStatusChanges() = runBlocking {
        var rx = 1
        val board = FakeBoard { c -> listOf(if (c == "status") """{"rx":$rx,"tx":0,"event":"x"}""" else "[]") }
        val api = CommandApi(LineTransport(board), "ble", "test", null)
        api.request("GET", "/api/status", null); api.request("GET", "/api/messages", null)
        api.request("GET", "/api/status", null); api.request("GET", "/api/messages", null)
        assertEquals(1, board.commands.count { it == "messages" }) // unchanged status: cached
        rx = 2
        api.request("GET", "/api/status", null); api.request("GET", "/api/messages", null)
        assertEquals(2, board.commands.count { it == "messages" })
    }

    @Test fun petIsReadFreshAndItsActionsGoAsCommands() = runBlocking {
        // The pet's JSON follows a log line; its look changes every poll, so BLE does not cache it.
        val board = FakeBoard { c -> when {
            c == "pet" -> listOf("I (123) wifi: log", """{"stage":"baby","sprite":"0120","graves":[]}""")
            c == "pet cuddle" -> listOf("""OK Purr {"stage":"baby"}""")
            else -> listOf("""{"rx":1}""")
        } }
        val api = CommandApi(LineTransport(board), "ble", "test", null)
        val got = api.request("GET", "/api/pet", null)
        assertEquals(200, got.status)
        assertTrue(got.body.contains("sprite"))
        api.request("GET", "/api/pet", null)
        assertEquals(2, board.commands.count { it == "pet" })
        assertEquals(200, api.request("POST", "/api/command", """{"command":"pet cuddle"}""").status)
    }

    @Test fun diceAreReadFreshAndTheirActionsGoAsCommands() = runBlocking {
        // Rolls change on the device's screen too: GET /api/dice runs "dice" each time; an action answers "OK ... {json}".
        val board = FakeBoard { c -> when {
            c == "dice" -> listOf("""{"mode":"rpg","results":[],"pool":{"text":"2d6"}}""")
            c == "dice roll 2d6+1" -> listOf("""OK rolled {"mode":"rpg","results":[{"n":1,"formula":"2d6+1","parts":[12,8,5],"total":6}]}""")
            c == "dice roll 2d" -> listOf("ERR formula: NdX+NdX-N, X 2..1000, d% d66, up to 8 terms")
            else -> listOf("""{"rx":1}""")
        } }
        val api = CommandApi(LineTransport(board), "ble", "test", null)
        assertEquals(200, api.request("GET", "/api/dice", null).status)
        api.request("GET", "/api/dice", null)
        assertEquals(2, board.commands.count { it == "dice" })
        val rolled = api.request("POST", "/api/command", """{"command":"dice roll 2d6+1"}""")
        assertEquals(200, rolled.status)
        assertTrue(rolled.body.contains("\"total\":6"))
        assertEquals(400, api.request("POST", "/api/command", """{"command":"dice roll 2d"}""").status)
    }

    @Test fun channelsGoAsCommandsAndListRefreshesAfterChange() = runBlocking {
        val board = FakeBoard { c -> listOf(when {
            c == "status" -> """{"rx":1,"channels":1}"""
            c == "channels" -> """{"max":8,"channels":[]}"""
            c.contains("\"probe\"") -> """{"name":"#test","opened":0}"""
            c.contains("\"link\"") -> "ERR link: meshcore://channel/add?name=...&secret=<32 hex>"
            else -> "OK channel added 1A2B"
        }) }
        val api = CommandApi(LineTransport(board), "ble", "test", null)
        api.request("GET", "/api/status", null)
        assertEquals(200, api.request("GET", "/api/channels", null).status)
        assertEquals(200, api.request("GET", "/api/channels", null).status)
        assertEquals(1, board.commands.count { it == "channels" }) // BLE: cached
        // Pretty-printed JSON from the page becomes one command line.
        assertEquals(200, api.request("POST", "/api/channels", "{\n \"action\": \"add\",\n \"hashtag\": \"#test\"\n}").status)
        val sent = board.commands.last()
        assertTrue(sent.startsWith("channel do {") && !sent.contains('\n'))
        assertEquals("#test", JSONObject(sent.removePrefix("channel do ")).getString("hashtag"))
        api.request("GET", "/api/channels", null)
        assertEquals(2, board.commands.count { it == "channels" }) // the change rereads the list
        assertEquals(400, api.request("POST", "/api/channels", """{"action":"add","link":"x"}""").status)
        val probe = api.request("POST", "/api/channels", """{"action":"probe","hashtag":"#test"}""")
        assertEquals(200, probe.status)
        assertEquals("#test", JSONObject(probe.body).getString("name"))
    }

    @Test fun channelsOnOlderFirmwareAreNamed() = runBlocking {
        val board = FakeBoard { listOf("Commands: status, config, key, messages, radar") }
        val api = CommandApi(LineTransport(board), "usb", "test", null)
        assertEquals(501, api.request("GET", "/api/channels", null).status)
        assertEquals(501, api.request("POST", "/api/channels", """{"action":"add","hashtag":"#test"}""").status)
    }
}
