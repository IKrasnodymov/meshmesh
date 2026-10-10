package org.meshmesh.app.api

import kotlinx.coroutines.CancellationException
import org.json.JSONObject
import org.meshmesh.app.link.GarbledAnswerException
import org.meshmesh.app.link.LineTransport
import org.meshmesh.app.link.LinkClosedException
import java.io.IOException
import java.util.Base64

/**
 * The page's HTTP API over the board's command line (USB, BLE, TCP bridge): each request becomes
 * the command the device's own HTTP handler runs (src/Portal.cpp) and gets the same status codes.
 */
class CommandApi(
    private val transport: LineTransport,
    override val kind: String,
    override val label: String,
    private val tiles: TileStore?,
    /** Before a timed-out command is retried (USB: back to 115200 baud). Returns true to retry. */
    private val recover: (suspend () -> Boolean)? = null,
    /** While waiting for a long answer (native USB wake-up). */
    private val whileWaiting: (suspend () -> Unit)? = null,
) : DeviceApi {
    override var onLost: (String) -> Unit = {}
    private val ble = kind == "ble"
    private val timeout = if (ble) 45_000L else 20_000L
    // BLE is slow (kilobytes per second): lists are reread when the status shows a change or after 20 s.
    private val cache = HashMap<String, Pair<Long, String>>()
    private var statusPrint = ""

    init {
        transport.onClosed = { reason -> onLost(reason) }
    }

    suspend fun run(command: String): String {
        val reply = exchange(command)
        // Firmware before the app's commands answers them with its help line.
        if (reply.startsWith("Commands:") && APP_COMMANDS.any { command.startsWith(it) }) throw OldFirmwareException()
        return reply
    }

    private suspend fun exchange(command: String): String {
        try {
            return transport.exchange(command, timeout, whileWaiting)
        } catch (e: GarbledAnswerException) {
            return transport.exchange(command, timeout, whileWaiting)
        } catch (e: IOException) {
            if (e is LinkClosedException || recover == null || !recover.invoke()) throw e
            return transport.exchange(command, timeout, whileWaiting)
        }
    }

    override suspend fun request(method: String, path: String, body: String?): ApiReply {
        val (route, query) = splitPath(path)
        return try {
            if (method == "GET") get(route, query) else post(route, body ?: "")
        } catch (e: CancellationException) {
            throw e
        } catch (e: LinkClosedException) {
            ApiReply.failed(e.message ?: "Связь потеряна")
        } catch (e: OldFirmwareException) {
            ApiReply(501, e.message!!)
        } catch (e: IOException) {
            ApiReply(504, e.message ?: "Нет ответа устройства")
        } catch (e: Exception) { // malformed request body from the page
            ApiReply(400, "ERR ${e.message}")
        }
    }

    private suspend fun get(route: String, query: Map<String, String>): ApiReply {
        val command = when (route) {
            "/api/maps/tile" -> return tile(query)
            "/api/tour" -> "tour"
            "/api/chess" -> if (query.containsKey("rating")) "chess rating" else query["id"]?.let { "chess show $it" } ?: "chess web"
            "/api/radar" -> "radar web"
            "/api/wardrive/log" -> "wardrive log " + (if (query["kind"] == "nets") "nets" else "mesh") + " " + (query["from"]?.toIntOrNull()?.coerceAtLeast(0) ?: 0)
            else -> GETS[route] ?: return ApiReply(404, "Not found")
        }
        val reply = cached(command)
        return if (reply.startsWith("ERR") || reply.startsWith("Commands:")) ApiReply(400, if (route == "/api/chess") "Unknown game" else reply)
        else ApiReply(200, reply)
    }

    private suspend fun cached(command: String): String {
        if (!ble || command !in SLOW) {
            val reply = run(command)
            if (command == "status") noteStatus(reply)
            return reply
        }
        cache[command]?.let { (at, value) -> if (System.currentTimeMillis() - at < 20_000) return value }
        val reply = run(command)
        if (!reply.startsWith("ERR")) cache[command] = System.currentTimeMillis() to reply
        return reply
    }

    private fun noteStatus(json: String) {
        val print = runCatching {
            val s = JSONObject(json)
            STATUS_PRINT.joinToString("|") { s.opt(it)?.toString() ?: "" }
        }.getOrDefault(json)
        if (print != statusPrint) { statusPrint = print; cache.clear() }
    }

    private suspend fun post(route: String, body: String): ApiReply {
        cache.clear()
        when (route) {
            "/api/command" -> {
                val command = JSONObject(body).getString("command")
                val reply = run(command)
                return ApiReply(if (reply.startsWith("ERR")) 400 else 200, reply)
            }
            "/api/send" -> {
                val j = JSONObject(body)
                val to = j.getString("to"); val text = j.getString("text")
                // A line break would end the command line; such text goes as JSON.
                val command = if (text.contains('\n') || text.contains('\r')) "sendjson " + JSONObject().put("to", to).put("text", text) else "send $to $text"
                return ok(run(command))
            }
            "/api/config" -> return ok(run("set " + JSONObject(body)))
            "/api/people" -> return ok(run("people do " + JSONObject(body)))
            "/api/quick" -> return ok(run("quick do " + JSONObject(body)))
            "/api/radar" -> return ok(run("radar do " + JSONObject(body)))
            "/api/wardrive" -> return ok(run("wardrive do " + JSONObject(body)))
            "/api/channels" -> {
                val reply = run("channel do " + JSONObject(body))
                return ApiReply(if (reply.startsWith("ERR")) 400 else 200, reply) // "probe" answers JSON
            }
            "/api/maps/chunk" -> return chunk(JSONObject(body).getString("data"))
        }
        return ApiReply(404, "Not found")
    }

    private fun ok(reply: String) = ApiReply(if (reply.startsWith("OK")) 200 else 400, reply)

    /** The page sends up to 2048 bytes; the board takes 640 per USB line and 183 per BLE write. */
    private suspend fun chunk(data: String): ApiReply {
        val bytes = Base64.getDecoder().decode(data)
        val step = if (ble) 180 else 480
        var at = 0
        var reply = "OK map chunk"
        while (at < bytes.size) {
            val part = bytes.copyOfRange(at, minOf(bytes.size, at + step))
            reply = run("map chunk " + Base64.getEncoder().encodeToString(part))
            if (!reply.startsWith("OK")) return ApiReply(400, reply)
            at += step
        }
        return ApiReply(200, reply)
    }

    /** A saved map tile, read in parts ("map tile Z X Y OFFSET LENGTH"); unchanged tiles come from the phone. */
    private suspend fun tile(query: Map<String, String>): ApiReply {
        val z = query["z"]?.toIntOrNull(); val x = query["x"]?.toLongOrNull(); val y = query["y"]?.toLongOrNull()
        if (z == null || x == null || y == null) return ApiReply(400, "ERR tile coordinates")
        val head = run("map tile $z $x $y 0 $TILE_STEP")
        if (head.startsWith("ERR")) return ApiReply(404, "Map tile not saved")
        val first = TilePart.parse(head) ?: part(z, x, y, 0) ?: return ApiReply(500, "Ошибка чтения карты")
        tiles?.get(z, x, y)?.let { saved ->
            if (saved.size == first.size && first.data.size >= 24 && saved.copyOfRange(0, 24).contentEquals(first.data.copyOfRange(0, 24)))
                return ApiReply(200, Base64.getEncoder().encodeToString(saved), true)
        }
        val out = java.io.ByteArrayOutputStream(first.size)
        out.write(first.data)
        while (out.size() < first.size) {
            val part = part(z, x, y, out.size()) ?: return ApiReply(500, "Ошибка чтения карты")
            if (part.offset != out.size() || part.size != first.size || part.data.isEmpty()) return ApiReply(500, "Карта изменилась во время чтения")
            out.write(part.data)
        }
        val data = out.toByteArray()
        tiles?.put(z, x, y, data)
        return ApiReply(200, Base64.getEncoder().encodeToString(data), true)
    }

    /** One part of a tile; a part cut by a driver log line is read again. */
    private suspend fun part(z: Int, x: Long, y: Long, offset: Int): TilePart? {
        repeat(2) { TilePart.parse(run("map tile $z $x $y $offset $TILE_STEP"))?.let { return it } }
        return null
    }

    override fun close() = transport.close("Отключено")

    companion object {
        const val TILE_STEP = 6144
        val GETS = mapOf(
            "/api/quick" to "quick",
            "/api/people" to "people", "/api/status" to "status", "/api/messages" to "messages", "/api/nodes" to "nodes",
            "/api/config" to "config", "/api/key" to "key", "/api/navigation" to "navigation",
            "/api/maps" to "map info", "/api/maps/areas" to "map areas", "/api/clock" to "clock",
            "/api/connections" to "connections", "/api/channels" to "channels", "/api/pet" to "pet",
            "/api/dice" to "dice", "/api/wardrive" to "wardrive",
        )
        /** Commands added for the app (docs/android.md): older firmware lacks them. */
        private val APP_COMMANDS = listOf("quick", "people", "radar web", "radar do ", "map tile ", "connections", "sendjson ", "channels", "channel do ", "wardrive")
        private val SLOW = setOf("messages", "nodes", "config", "chess web", "chess rating", "tour", "map areas", "channels")
        private val STATUS_PRINT = listOf("boot", "tx", "rx", "event", "relayed", "rejected", "contacts_replaced", "wifi", "ble", "busy", "channels")
    }
}

class OldFirmwareException : IOException("Прошивка устройства старее приложения: эта функция по USB и Bluetooth появится после обновления MeshMesh")

/** "OK tile SIZE OFFSET BASE64" */
data class TilePart(val size: Int, val offset: Int, val data: ByteArray) {
    companion object {
        fun parse(line: String): TilePart? {
            if (!line.startsWith("OK tile ")) return null
            val parts = line.split(' ', limit = 5)
            if (parts.size < 4) return null
            val size = parts[2].toIntOrNull() ?: return null
            val offset = parts[3].toIntOrNull() ?: return null
            val data = runCatching { Base64.getDecoder().decode(parts.getOrElse(4) { "" }.trim()) }.getOrNull() ?: return null
            return TilePart(size, offset, data)
        }
    }
}
